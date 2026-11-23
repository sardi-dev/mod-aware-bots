# Mage: Fire (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). The solo hard-cast
  Pyroblast opener (priority line 5) is done; Presence of Mind and Alter Time into Pyroblast are left for later (no
  actions yet). The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Fire Mage Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 22 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the talent hints in the text are usable).
  - wowtbc.gg, "Fire Mage Rotation & Talent Build" in the MoP class guides (MoP Classic, no patch line on the page), read
    2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent,
    GlyphProperties, SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellRadius, SpellAuraOptions;
    `data/db2/Item-sparse.db2` for glyph item ids) through `spec_data.py` and short read-only scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, Icy Veins retried 2026-11-11); Wowhead needs a browser
    (its leveling page answered 404).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the
  tank's marks), 61 to 90 briefly from the guides. A section on low level duels and kiting is included (owner report of
  2026-11-11).

Which source was richer: **talents (named core picks, the dungeon bomb debate), glyphs for dungeons, stat order, the
opener** Method (wowtbc lists every talent option without a pick and has no stat order); **the 2 to 4 and 5 plus target
split, the Scorch-while-moving step, the defensive order** wowtbc; **levels 15 to 60, what to do before Inferno Blast or
Scorch is learned, duels** neither guide says anything (both are level 90 raid guides), so those lines come from the 5.4.8
data and are marked "derived". The two guides agree on the single target order and differ only in the glyph names for the
third slot and in wording (Method puts Haste over Mastery and Crit above both; wowtbc has no stat order).

## Talents

Tiers and columns are from Talent.dbc (the three mage specialisations share one talent list). "Bots today" is
`AiPlayerbot.PremadeSpecLink.8.1 = 000000` (`conf/playerbots.conf.dist:2849`; one digit per tier, column counted from 0):
Presence of Mind, Temporal Shield, Ring of Frost, Greater Invisibility, Nether Tempest, Invocation.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (000000) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Presence of Mind, a core pick with Invocation and Living Bomb (Method); instant Pyroblast in the opener and on Alter Time (both guides) | Ice Floes, Blazing Speed | 12043 / 108839 / 108843 | Presence of Mind, same. **Dead today:** the `presence of mind` trigger is registered (`MageAiObjectContext.cpp:106`) and the action class exists (`MageActions.h:408-412`) but no action creator is registered, so it is never cast |
| 2 (30) | Temporal Shield, a strong cooldown (Method) | Ice Barrier, the simpler option (Method); Flameglow | 115610 / 11426 / 140468 | Temporal Shield, same; edge `being attacked` -> `temporal shield` (`GenericMageStrategy.cpp:219`) |
| 3 (45) | Ring of Frost (wowtbc lists it first; Method names no pick for Fire) | Ice Ward, Frostjaw | 113724 / 111264 / 102051 | Ring of Frost. **Dead today:** no `ring of frost` action |
| 4 (60) | Cauterize, rated high value (Method) | Greater Invisibility, better against big damage (Method); Cold Snap | 86949 / 110959 / 11958 | Greater Invisibility; its only edge casts `invisibility` (`GenericMageStrategy.cpp:195`); to check live whether that name still resolves once the talent replaces Invisibility |
| 5 (75) | Living Bomb on one target; Nether Tempest as the dungeon compromise (Method); Frost Bomb only with 5 or more targets | | 44457 / 114923 / 112948 | Nether Tempest, via the Living Bomb nodes' fallback (`FireMageStrategy.cpp:68-81`, `:116`) |
| 6 (90) | Invocation (Method's core build) | Rune of Power, Incanter's Ward (wowtbc lists all) | 114003 / 116011 / 1463 | Invocation, same |

Notes on the picks (data, not guide):

- Tiers 1 to 4 are the ones that matter for levels 15 to 60. Presence of Mind, Ring of Frost and a Greater Invisibility cast
  that may not resolve are dead today; only Temporal Shield works. **Cauterize** (86949) is passive ("an attack which would
  otherwise kill you brings you to a part of your health", spell text) and needs no code: the safer tier 60 pick until the
  Invisibility action is checked (derived).
- Presence of Mind (12043): the next Mage spell with a cast time under 10 s becomes instant, 90 s cooldown, off the global
  cooldown. For Fire it makes Pyroblast (3.5 s cast) an instant: the guides' burst. The bot casts Pyroblast only with the
  "Pyroblast!" proc (`FireMageStrategy.cpp:114`), so with an action `presence of mind` followed by `pyroblast` the pick would
  start to pay (rotation item 5). Until then Blazing Speed is the only tier 1 talent with code, and only in duels and
  battlegrounds (`GenericMageStrategy.cpp:225-228`).
- Ice Barrier (11426): absorbs damage for 60 s and "spellcasting will not be delayed by damage" (spell text), 25 s cooldown.
  The Fire strategy has an edge for it (`FireMageStrategy.cpp:112`, `medium health`), which never fires with Temporal Shield
  picked; Method's simpler option is therefore a working alternative for a bot (derived).
- Invocation (114003): Evocation has no cooldown and a shorter channel, and a finished Evocation raises spell damage while
  lowering mana regeneration (spell text). Both guides use Evocation to keep that buff up (Method: pre-pull, wowtbc: step 6);
  the bots only cast Evocation at low mana (`GenericMageStrategy.cpp:196`), so for them Invocation is a cheaper emergency
  Evocation (derived). Level 90 only.

Proposed link, guide aligned and safe for the code that exists: `000100` (Presence of Mind / Temporal Shield / Ring of Frost /
Cauterize / Nether Tempest / Invocation); only the tier 60 digit changes (owner question 2). For a character below 75 only
the first four digits are in effect.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and
75 (config comment `conf/playerbots.conf.dist:2631-2636`; the unlock levels are core-side). The config order is major,
minor, major, minor, minor, major; a glyph goes to the first free slot of its type. So the first major and first minor are
the only ones in effect from 25 to 49, the second pair from 50 to 74.

| Type | Glyph | Item id | Why (one clause) | Bots today (`42739,45739,63539,42743,42735,42748`) |
|------|-------|--------:|------------------|------------------------|
| Major | Combustion | 42739 | Method and wowtbc best set; doubles direct damage, DoT duration and cooldown of Combustion (spell text), learned at 77 | yes, in **slot 1 (level 25) where it does nothing until 77** |
| Major | Inferno Blast | 63539 | Method and wowtbc best set; Inferno Blast spreads to one more target; learned at 24 | yes (slot 2, level 50) |
| Major | Rapid Displacement | 42748 | Method raid best; Blink gets 2 charges but no longer frees from stuns, Blink is learned at 7 | yes, in slot 3 (level 75) |
| Major, situational | Cone of Cold | 42746 or 62764 | Method dungeon best (cleave and AoE); the bots never cast Cone of Cold for Fire, see the gap list | no. Two items carry this name in the item data; which one teaches the 5.4.8 glyph (115705, +damage) is to check in game |
| Major, situational | Arcane Explosion | 42736 | Method dungeon situational (extra AoE); not cast by the bots | no |
| Major, derived | Frost Nova | 42741 | the bots cast Nova every time an enemy is within 5 yd; 5 s less cooldown (effect value -5000) | no (in neither guide) |
| Minor | Momentum | 42743 | Method's only minor; Blink goes in the movement direction instead of the facing | yes (slot 2, level 50) |
| Minor | Mirror Image | 45739 | wowtbc; Mirror Images cast Fireball instead of Frostbolt; Mirror Image is learned at 49 | yes, in **slot 1 (level 25) where it does nothing until 49** |
| Minor | Loose Mana | 42735 | not in a guide; Mana Gem restores over time instead of at once | yes (slot 3, level 75) |

Facts checked in the core: with Glyph of Momentum, Blink picks the movement direction (`Spell.cpp:1489-1510`); a stationary
caster keeps its facing, so the bots' `blink back` (face away, then Blink: `MageActions.cpp:38-46`) still works while
standing. Rapid Displacement blocks Blink while stunned (`spell_mage.cpp:3106-3140`), the price of its second charge.

Disagreement: Method's raid majors are Rapid Displacement, Combustion, Inferno Blast and its dungeon majors Cone of Cold,
Combustion, Inferno Blast; wowtbc has Combustion, Inferno Blast, Rapid Displacement plus Cone of Cold for 5 or more targets;
the minor is Momentum (Method) or Mirror Image (wowtbc). The premade follows wowtbc. The slot order is derived: a glyph in a
slot must do something at the level the slot unlocks. Proposed `PremadeSpecGlyph.8.1`:
`63539,42743,42748,45739,42735,42739` = Inferno Blast, Momentum, Rapid Displacement, Mirror Image, Loose Mana, Combustion
(the same six glyphs, reordered: Combustion waits for slot 3 at 75, two levels before the spell).

## Stat order

Method: Intellect, then Hit and Expertise to 15 percent chance to hit, then Critical Strike, then Haste over Mastery.
wowtbc has no stat order. The caps are level 90 numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Intellect first.** It is the primary stat of the class and the only way an item adds damage in 5.x; Spell Power is not an
  item stat any more (inference: the data here does not list item stats).
- Then Critical Strike (Critical Mass, learned at 36, multiplies the crit chance of Fireball, Frostfire Bolt, Pyroblast and
  Scorch, and the whole Heating Up chain runs on crits), then Haste, then Hit (matters near 90), then Mastery last. Below
  level 80 Mastery does nothing for a Fire mage because Mastery: Ignite is learned at 80 (SpecializationSpells).
- Spirit is in neither guide. It only adds mana regeneration for a mage (inference); rank it at the bottom.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:542-551`, Fire only): Spell Power 1.0, Hit 1.2, Critical 1.1, Mastery 1.0,
  Haste 0.8, **Spirit 0.7, Intellect 0.3**. Against the guide: **Spirit outranks Intellect more than two to one** (a cloth item
  with Spirit beats one with Intellect), and **Mastery (1.0) is above Haste (0.8)**, which Method reverses (the code comment
  there calls the Mastery weight "unsourced/contested"). The Spirit penalty of 0.7 applies only until Molten Armor (level
  34) is known (`:712-720`); the rule came from a Wrath of the Lich King armor that turned Spirit into critical strike, which
  Molten Armor no longer does in 5.4.8 (spell text: critical strike chance and less physical damage taken).
- Weapons: a staff or a one-hander with an off-hand; the main-hand weight of a caster is cut to 0.65 (`:851-855`).

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts; the data holds old rows of the same name. Costs are percent of base mana
(SpellPower). The global cooldown is 1.5 s unless noted.

| Level | Spell (id) |
|------:|------------|
| 1 | Frostfire Bolt (44614, 2.75 s cast, 4 percent, slows 40 percent for 8 s; every mage has it) |
| 3 | Frost Nova (122, 2 percent, 25 s: roots enemies within 12 yd for up to 8 s, "damage caused may interrupt the effect") |
| 5 | Fire Blast (2136, instant, 8 s, 30 yd, 2 percent) |
| 7 | Blink (1953, 20 yd, 15 s, 2 percent, frees from stuns and bonds) |
| 8 | Counterspell (2139, 24 s, 40 yd, 2 percent, 6 s school lock, not on the global cooldown) |
| 10 | Pyroblast (11366, 3.5 s cast, 5 percent, a fire DoT for 18 s; two single-target direct-damage fire crits in a row make the next Pyroblast instant, free and stronger: the auras "Heating Up" 48107 after one crit and "Pyroblast!" 48108 after two); the specialisation is chosen here |
| 12 | Fireball (133, 2.25 s cast, 4 percent) |
| 14 | Polymorph (118, 1.7 s cast, 30 yd, up to 50 s) |
| 18 | Arcane Explosion (1449, instant, 3 percent, 10 yd; every mage has it) |
| 22 | Ice Lance (30455, instant, 1 percent; every mage has it) |
| 24 | Inferno Blast (108853, instant, 8 s, 40 yd, 2 percent, **always critical**, spreads Pyroblast, Ignite and Combustion to up to 3 nearby enemies; replaces Fire Blast) |
| 26 | Ice Block (45438, 10 s, 5 min, Hypothermia afterwards) |
| 28 | Cone of Cold (120, instant, 4 percent, 10 s, slows 60 percent for 6 s; every mage has it) |
| 29 | Remove Curse (475, 8 s) |
| 32 | Slow Fall (130) |
| 34 | Molten Armor (30482, 3 s cast, critical strike up and physical damage taken down) |
| 36 | Critical Mass (117216, passive: crit chance of Fireball, Frostfire Bolt, Pyroblast, Scorch multiplied) |
| 38 | Conjure Refreshment (42955, 3 s cast) |
| 40 | Evocation (12051, 120 s: 15 percent of mana at once and another 45 percent over the 6 s channel, spell text) |
| 44 | Flamestrike (2120, 2 s cast, 6 percent, 8 yd, fire DoT, and an enemy-slowing blast wave because Pyroblast is known: -50 percent for 3 s) |
| 47 | Conjure Mana Gem (759, 3 s cast, 18 percent; makes item 36799) |
| 48 | Scorch (2948, 1.5 s cast, 2 percent, "can be cast while moving") |
| 49 | Mirror Image (55342, 3 min, 30 s) |
| 50 | Wizardry (89744, +5 percent Intellect) |
| 52 | Blizzard (10, 5 percent, 8 s channel, 8 yd, slows 50 percent) |
| 54 | Frost Armor (7302) |
| 56 | Invisibility (66, 5 min) |
| 58 | Arcane Brilliance (1459, party-wide spell power and critical strike) |
| 60 | tier 4 talent |
| 62 | Dragon's Breath (31661, instant, 4 percent, 20 s: a cone disorients for 4 s, "any direct damaging attack will revive targets") |
| 64 | Spellsteal (30449) |
| 70 | Improved Counterspell (12598: adds a silence) |
| 75 | tier 5 talent (bombs) |
| 77 | Combustion (11129, 45 s, 10 percent: instant damage, stun, and the target's Ignite burns again; resets Inferno Blast) |
| 80 | Mastery: Ignite (12846); Mage Armor (6117) |
| 84 | Time Warp (80353, 5 min) |
| 85 | Pyromaniac (132209) |
| 87 | Alter Time (108978, 3 min) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and data tables):

- The Fire burst is a crit chain: any direct fire crit gives "Heating Up"; a second crit while it is up gives "Pyroblast!"
  (instant, free, stronger Pyroblast). Inferno Blast always crits, so Inferno Blast on "Heating Up" always produces
  "Pyroblast!". Ignite (the DoT on crits) comes with the Mastery at 80, so before 80 Fire is the chain and the filler
  only; Combustion (77) needs an Ignite to be worth casting.
- Inferno Blast is learned at 24; from 5 to 23 the instant is Fire Blast (8 s, not a guaranteed crit).
- Scorch (48) is the moving filler; Inferno Blast and Fire Blast are instants too.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` at commit `5927134`.

How the bots rank what they do (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, ACTION_NORMAL 10, ACTION_HIGH 20,
ACTION_MOVE 30, ACTION_INTERRUPT 40, ACTION_EMERGENCY 90. A trigger edge adds its action with that number; the engine takes
the highest action that is possible and useful; an action whose spell is unknown is simply skipped. A Fire mage gets the
`fire` and `fire aoe` combat strategies plus `dps`, `dps assist`, `cure`, `cc` and `aoe` (`Factory/AiFactory.cpp:353-366`; the
`frostfire` branch needs the Ice Shards talent aura, which 5.4.8 does not have, so it never runs), and every bot outside
battlegrounds gets `boost`; non-combat it gets `bdps` (the armor strategy) and `cure` (`AiFactory.cpp:722-729`).
`BoostTrigger` (`GenericTriggers.cpp:416-424`) is active at once against a player, otherwise only when the group is
out-matched 2 to 1 (balance 50 or less; inference from the rogue sheet, to check live), so a normal 5-player pull never gets
the Mirror Image edge in `boost`.

### Levels 15 to 29

Known: Frostfire Bolt, Frost Nova, Fire Blast (5 to 23), Blink, Counterspell, Pyroblast (10), Fireball (12), Polymorph,
Arcane Explosion (18), Ice Lance (22), Inferno Blast (24), Ice Block (26), Cone of Cold (28), Remove Curse (29).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | "Pyroblast!" up | Pyroblast (11366, 10), instant | yes, 50 (`FireMageStrategy.cpp:114`) |
| 2 | core | "Heating Up" up | Inferno Blast (108853, 24); before 24 Fire Blast (2136, 5) | yes from 24, 40 (`:113`); **missing from 10 to 23:** the `inferno blast` node has no alternative (`:44-49`), so Fire Blast is never cast and the chain relies on Fireball crits alone |
| 3 | core | filler | Fireball (133, 12) | yes, default 5.6 (`:104`) |
| 4 | core | moving | Inferno Blast / Fire Blast, then Scorch from 48 | Inferno Blast yes (default 5.8, `:102`); Fire Blast missing; Scorch the last default (`:105`) |
| 5 | good | opener, enemy not yet in combat | hard-cast Pyroblast (11366, 10) (both guides: "pre-cast Pyroblast") | missing: the default list has no hard-cast Pyroblast, and the mage pull strategy (used only when a pull is requested) casts `shoot` (`MageAiObjectContext.cpp:47`, a wand spell) |
| 6 | good | Inferno Blast ready, no "Heating Up" | do not spend it (Method: "when Heating Up is active") | differs: it is also a default action at 5.8 (`:102`), so it is cast on cooldown whether or not "Heating Up" is up (derived: the guaranteed crit is worth more as the conversion of a natural crit) |
| 7 | good | an enemy within 5 yd | Frost Nova (122, 3), then step away | yes, 50 (`GenericMageStrategy.cpp:210-211`) |
| 8 | good | the enemy is frozen and close | Blink (1953, 7) away, then resume | yes, `blink back` 35 (`:212-213`) |
| 9 | good | a target casts, from 8 | Counterspell (2139, 8) | **missing:** only an enemy healer (`:187`) and, in a battleground, arena or started duel, any enemy caster (`:221-222`); the plain `counterspell` trigger is registered (`MageAiObjectContext.cpp:108`) but no strategy schedules it |
| 10 | good | a second attacker or a marked target, from 14 | Polymorph (118, 14) | yes, `cc` 30 (`:284`; BT-028, live check pending) |
| 11 | good | 3 or more enemies within 8 yd of the target, from 18 | Arcane Explosion (1449, 18), 10 yd | **missing:** the AoE edges start at Flamestrike (44) and Blizzard (52) |
| 12 | good | an enemy in melee to slow, from 28 | Cone of Cold (120, 28) | **missing for Fire** (Method: spam it with Arcane Explosion at 5 or more targets) |
| 13 | expert | health at or under 25 percent | Ice Block (45438, 26) | yes, 90 (`:193`); in a started duel only in a group at 10 percent (`MageTriggers.cpp:192-199`) |
| 14 | good | a party member is cursed | Remove Curse (475, 29) | yes, `cure` 41 and 40 (`:243-247`) |

Before a spell is learned (derived): below 10 there is no specialisation and the kit is Frostfire Bolt, Frost Nova, Fire
Blast, Blink and Counterspell; the bot gets the `frost` strategy then (`AiFactory.cpp:280-287`), which is harmless. From 10
to 11 a Fire mage has Pyroblast (3.5 s) and Fire Blast only; Fireball waits for 12.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 15 | core | out of combat, armor missing | Molten Armor (30482, 34) (Method: "maintain Molten Armor") | yes (`MageTriggers.cpp:49-52`, node `GenericMageNonCombatStrategy.cpp:71`); the only armor edge the Fire mage needs |
| 16 | core | always | Critical Mass (36) | passive, nothing to do |
| 17 | core | low mana | Evocation (12051, 40), then a mana potion | yes, 90 (`GenericMageStrategy.cpp:196`) |
| 18 | good | no conjured food in the bags, from 38 | Conjure Refreshment (42955, 38) | yes (`GenericMageNonCombatStrategy.cpp:62`, BT-029) |
| 19 | good | 3 or more enemies within 8 yd | Flamestrike (2120, 44), then Blizzard | yes, 23 and 22 (`GenericMageStrategy.cpp:304-310`, `FireMageStrategy.cpp:122-130`) |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | core | moving | Scorch (2948, 48) | yes, last default (`FireMageStrategy.cpp:105`) |
| 21 | good | mana at the "high mana" threshold, gem in the bags, from 47 | Conjure Mana Gem (759, 47), then use the gem | yes, but only if the spell is known when the strategy is built (`GenericMageStrategy.cpp:239-240`; inference, to check) |
| 22 | good | on a pack of elites or a boss, from 49 | Mirror Image (55342, 49) on cooldown (both guides) | partly: `boost` 17.5 (`GenericMageStrategy.cpp:271`), gated by the balance rule above |
| 23 | good | 3 or more enemies, from 52 | Blizzard (10, 52), cancelled when fewer than 2 enemies are left | yes (`GenericMageStrategy.cpp:289`) |
| 24 | good | two attackers, from 56 | Invisibility (66, 56) as a threat dump (derived) | yes, 30 (`:195`, `MageTriggers.cpp:202-209`); a 5 minute cooldown |
| 25 | core | out of combat, party buff missing, from 58 | Arcane Brilliance (1459, 58) | yes, 21 (`GenericMageNonCombatStrategy.cpp:55`) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 26 | good | enemies in front within 10 yd, 3 or more | Dragon's Breath (31661, 62) | yes in the AoE list, 24 and 39 (`FireMageStrategy.cpp:126`, `GenericMageStrategy.cpp:305`) |
| 27 | good | "enemy too close for spell" | Dragon's Breath as a panic button | **wrong in a group (inference, to check live):** the edge fires at 41 for any current target within 5 yd that is not attacking the bot (`FireMageStrategy.cpp:117-119`, trigger `RangeTriggers.cpp:121-125`), which in a dungeon is the tank's target; the 4 s disorient on the tanked enemy moves it away from the tank |
| 28 | good | casters in range | Counterspell with silence (Improved Counterspell, 70) | missing, see line 9 |
| 29 | good | tier 4 talent | Cauterize passive, or Greater Invisibility | see Talents |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | Living Bomb on one target, Nether Tempest as the dungeon compromise, Frost Bomb at 5 or more targets (Method) | Living Bomb edge 19 with the Nether Tempest fallback (`FireMageStrategy.cpp:116`, `:68-81`); on attackers in the AoE strategy 21 (`:128`) |
| 77 | Combustion "on cooldown if the Ignite came from Pyroblast" (Method) | default action 5.9 (`FireMageStrategy.cpp:101`), the `combustion` edge 50 (`:115`) and `boost` 18: cast whenever ready, with or without a built Ignite; Ignite only exists from 80 |
| 80 | Mastery: Ignite; Mage Armor is not in either guide (Molten Armor is) | Molten Armor stays (`MageTriggers.cpp:49-52`): same as the guides |
| 84 | Time Warp on a big pull | missing (no action) |
| 87 | Alter Time with a free Pyroblast and Presence of Mind (both guides) | missing (no action) |
| 90 | single target (Method): Molten Armor, Invocation, Presence of Mind into Pyroblast, Alter Time, Combustion with a Pyroblast Ignite, Mirror Image, Living Bomb, Pyroblast with Heating Up, Inferno Blast on Heating Up, Fireball; area: spread bombs with Inferno Blast; over 4 targets Cone of Cold and Arcane Explosion with Frost Bomb | Molten Armor, Pyroblast!, Inferno Blast on Heating Up, Fireball, Living Bomb, Combustion, Mirror Image are there; Presence of Mind, Alter Time, Evocation to keep Invocation, Cone of Cold, Arcane Explosion are not |

## Cooldowns and defensives

The guides say "on cooldown"; the timing for a dungeon bot is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Presence of Mind | talent 15 (90 s) | on cooldown into Pyroblast | at a pack of elites or a boss | dead (no action) |
| Inferno Blast | 24 (8 s) | when Heating Up is active | the same | on Heating Up, and also on cooldown as a default (`:102`, `:113`) |
| Combustion | 77 (45 s) | after a Pyroblast Ignite | the same | on every ready cooldown |
| Mirror Image | 49 (3 min) | on cooldown | at a pack of elites or a boss | `boost`, balance rule (`GenericMageStrategy.cpp:271`) |
| Evocation | 40 (120 s) | pre-pull and to keep Invocation | low mana, when the tank holds the enemies | at low mana, 90 (`:196`) |
| Frost Nova / Blink | 3 / 7 | not covered | Nova a melee enemy in reach, Blink out | `GenericMageStrategy.cpp:210-213` |
| Ice Block | 26 (5 min) | first in wowtbc's defensive order | health under 25 to 35 percent with nothing else ready | 25 percent (`:193`); solo duel never |
| Temporal Shield / Ice Barrier | talent 30 | Method's pick / "simpler" option | when attacked | Temporal Shield 28.5 (`:219`); Ice Barrier only if picked |
| Cauterize / Greater Invisibility | talent 60 | Method: high value / better against big damage | death / threat dump | see Talents |
| Counterspell | 8 (24 s) | not covered | any enemy cast of a dangerous spell | healers and BG/duel casters only |
| Dragon's Breath | 62 (20 s) | not covered | AoE disorient only when nothing is tanked in front | see line 27 |

## Healer triage and mana (healers only)

Not applicable to healing. Mana: Fire spends 4 percent per 2.25 s on Fireball, about the same rate as Frost; Evocation arrives
at 40 and the Mana Gem at 47, so below 40 a long fight ends in a drink (derived). The mana lines the bots have are the same as
for the other mages: Evocation at 15 percent (`GenericMageStrategy.cpp:196`), the Mana Gem, the dungeon drink rule
(`GenericTriggers.cpp:50-60`, below 50 percent between pulls, config `AiPlayerbot.DungeonDrinkMana`) and the generic drink
at `LowMana` 15, `MediumMana` 40 (`conf/playerbots.conf.dist:674-675`).

## Low level duels and kiting (owner report 2026-11-11)

The guides are level 90 PvE guides and say nothing about Frostbolt's chill, Frost Nova, Blink or Ice Block at low levels;
wowtbc puts Ice Block first in its defensive order and nothing more. Everything below is derived from the 5.4.8 spell data.
For Fire the tools are:

- Burst: the Heating Up chain; Inferno Blast (24) is an instant guaranteed crit, Pyroblast! an instant free Pyroblast, and
  Combustion (77) stuns. Fire needs no distance to deal its damage, only uninterrupted casts (Ice Barrier removes pushback).
- Slow: Frostfire Bolt (1, all mages, movement -40 percent for 8 s, but not in the Fire list), Cone of Cold (28, -60 percent
  for 6 s), the Flamestrike wave (44, -50 percent for 3 s). Fire has no slow of its own before that.
- Control: Frost Nova (3, root 8 s), Counterspell (8), Polymorph (14), Blink (7, 20 yd, 15 s), Ice Block (26), Dragon's Breath
  (62, 4 s disorient).
- Sequence for a duel against a melee opponent (derived): Fireball from range; the enemy arrives; Nova at about 10 yd, then
  walk or Blink out of reach before dealing damage that may break the root; Inferno Blast on Heating Up, Pyroblast! on the
  proc; Counterspell its heal; Ice Block only when Nova and Blink are on cooldown.

What the bots do (read): in a started duel `in pvp instance` is true (`PvpRotationValues.cpp:189-195`), so the lane
`mage pvp melee enemy player` is active: Frost Nova 50, Blink back 35, Blazing Speed 34 when the enemy player is in melee
range (`GenericMageStrategy.cpp:225-228`), Counterspell on an enemy caster (`:221-222`), and the flee edge is off
(`MageTriggers.cpp:211-218`). The kite (`PvpKiteTrigger`, `RangeTriggers.cpp:63-119`, BT-145) runs only from a slowed
enemy (speed below 85 percent of the bot's) or steps out of reach of a rooted one. Where they fall short:

1. A Fire bot never slows its enemy (no Cone of Cold, no Frostfire Bolt in the list), so BT-145 keeps it standing: after Nova
   and Blink it trades damage at range 5 to 20 with whatever remains. For Fire this is acceptable (its burst is the answer),
   and the crit chain keeps working while standing.
2. The Nova trigger distance is the generic `TooCloseDistance` 5 yd (`conf/playerbots.conf.dist:646`) although Nova's radius
   is 12 yd and `CastFrostNovaAction::isUseful` allows 10 (`MageActions.cpp:24-28`): the enemy is already hitting the bot
   when the root lands.
3. A solo duelist never casts Ice Block: the duel counts as a pvp instance and the rule there is "in a group and health at or
   under 10 percent" (`MageTriggers.cpp:192-199`); everywhere else it is 25 percent.
4. Fire has no pushback protection by default (Ice Barrier is not picked): a melee enemy delays every Fireball.
5. Dragon's Breath at 62 is a good melee peel but it is fired by the generic "too close" edge and the AoE edge, not by a
   duel rule (line 27).

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Time Warp (84), Alter Time (87), the tier 5 bombs (75), Combustion (77) and the tier 6 talents
  (90) are not available in the 15 to 60 band; Mirror Image is learned at 49, Scorch at 48, Dragon's Breath at 62, Arcane
  Brilliance at 58, Molten Armor at 34. The lists above start without them.
- Inferno Blast (24) replaces Fire Blast only from 24; the guides do not mention the earlier instant.
- Method's "Combustion on cooldown if the Ignite comes from Pyroblast" needs Ignite, which is the Mastery learned at 80;
  from 77 to 79 Combustion has nothing to burn.
- Method's Fire stat order (Crit, then Haste over Mastery) fits the data: Mastery is only active from 80.
- The bots' pull action for a mage (used only when a pull is requested) is `shoot` (`MageAiObjectContext.cpp:47`); both guides open with a hard-cast Pyroblast
  instead. Shoot needs a wand (mages have the Wands skill in the data: SkillRaceClassInfo skill 228 with the mage class
  bit), which a leveling mage may not carry (to check).
- Spell Power is not an item stat in 5.x (inference); the bots' weight list still carries Spirit above Intellect.

## Gap list (ordered)

Config (can go at once):

1. Glyph order: `PremadeSpecGlyph.8.1` (`conf/playerbots.conf.dist:2851`) to `63539,42743,42748,45739,42735,42739`. Today a
   level 25 to 76 Fire bot carries Combustion (nothing before 77) and Mirror Image (nothing before 49) in its first slots.
2. Tier 60: `Cauterize` (digit 1): `PremadeSpecLink.8.1` (`:2849`) to `000100`. Reason: a passive cheat death needs no code,
   and the Invisibility cast is unverified. Owner question.
3. Tier 15 Presence of Mind and tier 45 Ring of Frost are dead talents; keep the guide picks only together with rotation
   items 5 and 9, otherwise Blazing Speed (digit 1 at tier 15) is the one with code. Owner question.

Rotation (class fixes, wait for the owner's in-game test):

1. Fire Blast for levels 10 to 23 (core): add `fire blast` as the alternative of the `inferno blast` node
   (`FireMageStrategy.cpp:44-49`) so the Heating Up edge (`:113`) works, and as a moving instant in the default list
   (`:99-106`). Today a Fire mage between 10 and 23 never casts its instant.
2. Counterspell on the current target's cast (good): add `TriggerNode("counterspell", ...)` at 40 to
   `GenericMageStrategy::InitTriggers` (`:187`), using the registered `CounterspellInterruptSpellTrigger`
   (`MageTriggers.h:62-66`). Applies to all three specialisations.
3. Dragon's Breath on "enemy too close for spell" (`FireMageStrategy.cpp:117-119`): restrict to an enemy that is attacking the
   bot or to an enemy player; in a group the edge currently reaches the tank's target (inference, to check live).
4. Cone of Cold (28) as the second slow and the 5 plus target spell, and Arcane Explosion (18) as the AoE filler before
   Flamestrike (44): `FireMageAoeStrategy::InitTriggers` (`FireMageStrategy.cpp:122-130`) and `MageAoeStrategy`
   (`GenericMageStrategy.cpp:302-312`). Guard against Polymorphed marked targets in the area (to check).
5. Presence of Mind action and an edge `presence of mind` then `pyroblast` on a pack of elites or a boss, only if the talent
   stays picked: register `CastPresenceOfMindAction` (`MageActions.h:408-412`) in `MageAiObjectContextInternal`
   (`MageAiObjectContext.cpp:224-308`), edge in `FireMageStrategy::InitTriggers`.
6. Hard-cast Pyroblast as the opener on the first enemy of a pack (good), instead of the `shoot` pull
   (`MageAiObjectContext.cpp:47`, `PullStrategy`): a mage pull action of `pyroblast` (or `fireball` below 10).
7. Inferno Blast as a default action (`FireMageStrategy.cpp:102`): gate it to "moving" or "Heating Up", so the guaranteed
   crit is spent on a conversion (derived, minor).
8. Armor is correct for Fire (Molten Armor from 34); nothing to fix. Before 34 the bot casts nothing (derived: no armor
   exists).
9. Mirror Image on a pack of elites or a boss (good): `MageBoostStrategy` (`GenericMageStrategy.cpp:271`) uses the balance
   gate; add a rule for 3 or more attackers or a boss in addition to "outmatched 2 to 1".
10. Duel Ice Block: `MageIceBlockHealthTrigger` (`MageTriggers.cpp:192-199`): a solo duelist never casts it. A rule for a
    started duel: health at or under 30 percent with Nova and Blink on cooldown. Nova distance for an enemy player 8 to 10 yd
    (PvP only), `GenericMageStrategy.cpp:210-211` or a new `mage pvp` trigger beside `:225-228`.
11. Ice Barrier: if the owner prefers it at tier 30, `being attacked` -> `ice barrier` exists for Frost only
    (`FrostMageStrategy.cpp:134`); add the same edge to Fire (the `medium health` edge `FireMageStrategy.cpp:112` is a heal-time
    cast, not an opener).
12. Stat weights (gear scoring): Intellect above Spirit and Spirit to 0, Haste above Mastery for Fire, Mastery last below
    level 80 (`StatsWeightCalculator.cpp:542-551`, `:712-720`). Applies to all three mage sheets.
13. Skill tiers: the mage strategies have no tier gating (`BotSkill` is used only in the tank code). Lines marked good and
    expert above (Counterspell, Polymorph, Blink-back, the Inferno Blast rule, Mirror Image timing, Ice Block) are the ones a
    casual bot should drop.
14. To check live: BT-027 (armor), BT-028 (Polymorph on a second attacker) and BT-029 (Conjure Refreshment) are deployed with
    their live check pending; whether the `invisibility` action resolves with Greater Invisibility; whether Frost Nova in a
    dungeon roots the tank's enemies and breaks the mage's own Polymorph (the Nova deals damage; read, not tested).

Owner questions: (1) Tier 15 for a bot with no Presence of Mind or Ice Floes code: keep the guide pick or Blazing Speed.
(2) Tier 60: Cauterize (no code) or Greater Invisibility (guide-listed, code unverified). (3) Tier 30 Fire: Temporal Shield
(Method's pick, works) or Ice Barrier (no pushback; edge needed). (4) May the first glyph slots hold a glyph the guide ranks
lower so that every slot works at its level. (5) Should a Fire mage hard-cast Pyroblast as its pull.

## PvP play (player guides, 5.x, read 2026-11-19)

- **Sources:** five player guides and clips by one arena and rated battleground Fire mage (patch 5.2 to 5.4; one is raw 2v2 commentary). No coach course. Facts only, nothing copied. Where a guide lacks material the line says GAP. The data checks are against this server's 5.4.8 tables.
- **Coverage:** strong on burst recipes and talents, thin on opener, positioning, interrupts, matchups and defensive use. The guide author says over 95 percent of top mages play Frost, so treat Fire as a niche pick.

**The plan.** Force a Hot Streak on purpose (Inferno Blast twice, 8 s apart with no other Fire spell between, or a frozen Scorch plus Inferno Blast, since Shatter doubles crit against frozen targets). Then stand a freeze window on the target (Frost Nova, Frostjaw or Ring of Frost, then Deep Freeze) and chain Pyroblast, Combustion, Pyroblast, Inferno Blast, Pyroblast: each crit on a frozen target feeds the next free Pyroblast, so two or three land inside one Deep Freeze. Control is the second half: sheep, Deep Freeze, Combustion stun, Frostjaw silence, Dragon's Breath.

**Burst variants (expert).** Alter Time duplicates a Hot Streak, so one proc gives two free Pyroblasts: Presence of Mind plus Alter Time, instant Frostjaw, Pyroblast with Deep Freeze on the same press, rewind before the Pyroblast lands, repeat, Combustion on the last Pyroblast. Presence of Mind on a Pyroblast does 25 percent less than a Hot Streak one, so spend it on a sheep or Frostjaw. If the healer trinkets a sheep, rewind and sheep again (Alter Time gives a second Presence of Mind). Without cooldowns: Frostjaw, two Scorches, Deep Freeze, three Pyroblasts (risky against interrupts).

**Defensives, interrupts, positioning.** GAP for interrupts and positioning. Defensives named: Temporal Shield (5.4 pick, used before the damage), Flameglow or Ice Barrier by comp (Ice Barrier is purgeable), Cold Snap for a second Ice Block, glyphs by matchup. Blink to reach a target is called inefficient (a good team stuns after it).

**Talents and glyphs, guides against the set** (`PremadeSpecLinkPvp.8.1 = 000212`):

| Tier | Guides | Set | Ladder (4 pooled) |
|---|---|---|---|
| 15 | Presence of Mind (Blazing Speed in battlegrounds) | Presence of Mind | split |
| 30 | Temporal Shield (5.4); Ice Barrier (5.2); Flameglow against purge comps | Temporal Shield | Flameglow 3 of 4 |
| 45 | Frostjaw for range setups; Ring of Frost for group fights | Ring of Frost | Frostjaw 3 of 4 |
| 60 | Cold Snap | Cold Snap | 4 of 4 |
| 75 | Living Bomb (Nether Tempest for groups; it wakes sheep) | Living Bomb | 4 of 4 |
| 90 | Incanter's Ward | Incanter's Ward | 4 of 4 |

Glyphs the guides want: Deep Freeze (not in the set, ladder 100 percent), Polymorph (in the set), then Armors against melee or Ice Block against casters, Remove Curse against Affliction, Spellsteal in mirrors. Stat order (steers rolls only): hit to 6 percent, haste to a breakpoint, crit, PvP power, mastery last; the 5.2 guide puts crit first with a 15 percent floor. Both agree mastery is last.

**Differences from the guides (5.4.8).** Glyph of Remove Curse gives 5 percent damage, not 15 (spell 115701). Incanter's Ward gives 15 percent spell damage after it breaks (the 5.2 guide says 30). Inferno Blast spreads Pyroblast, Ignite and Combustion; the guide's note that bombs are excluded is not in the spell text. Combustion also stuns and resets Inferno Blast (so it shares stun diminishing returns). Glyph of Rapid Displacement (146659) turns Blink into two charges that no longer free the caster from stuns; no guide mentions it. Level gates: Inferno Blast 24, Scorch 48, Dragon's Breath 62, Combustion 77, Alter Time 87, Deep Freeze 66, Frostjaw and Ring of Frost are talents.

**Bots today (read from `src/strategy/Classes/mage/`, ordered by value).**
1. No Deep Freeze window for Fire: Deep Freeze is a Frost edge only; there is no freeze, Pyroblast, Combustion, Pyroblast, Inferno Blast chain (`FireMageStrategy.cpp`, no `deep freeze` match).
2. No healer lock chain (Deep Freeze or Counterspell, then Polymorph or Ring of Frost); Polymorph runs only on a dungeon crowd control mark (`GenericMageStrategy.cpp:136`). Counterspell on the healer exists (`:190`).
3. Presence of Mind is spent on an instant Pyroblast (`FireMageStrategy.cpp:123`), which the guides call the weaker use; its trigger counts any player target as a big fight (`MageTriggers.cpp:304-320`).
4. Heating Up gives Inferno Blast (`:116`) and Pyroblast! gives Pyroblast (`:117`), but nothing stops a non-crit Fire spell between two planned crits, and the default fillers are Fireball and Scorch (`:103-104`).
5. Alter Time fires on a free Pyroblast, level 87, good tier, big fight (`:124`, `MageTriggers.cpp:322-328`); the rewind is left to the 6 s timer. Combustion has a default, a trigger at 50 and a boost edge, none timed to a window (`:101,125`).
6. The set holds Ring of Frost, which no code casts, while the code casts Frostjaw by default (`:100`) with the talent missing.
7. Dragon's Breath as a peel exists (`:128-129`); Temporal Shield "being attacked" exists (`GenericMageStrategy.cpp:222`); Ice Block, Spellsteal, Counterspell on casters, Frost Nova and Blink back exist (`:188-234`). The shared PvP layer (defensive and control-break picker, interrupt gate, fake cast) covers Ice Barrier and Ice Block.

**Research update (public patch notes 5.1 to 5.4.7 and spell data, 2026-11-19; lines marked derived come from game knowledge and the 5.4.8 data).** Glyph of Remove Curse was cut from 15 to 5 percent in 5.4.2, Flameglow absorbs 15 percent of spell power per hit since 5.3, Living Bomb and Nether Tempest deal 85 percent to players since 5.4.0, and Temporal Shield's heal follows Battle Fatigue since 5.4.2. Data: Inferno Blast 8 s, 40 yd; Combustion 45 s, instant, 40 yd; Frostjaw 20 s, 30 yd, 1.5 s cast (hence Presence of Mind), root and silence 8 s and half on players; Deep Freeze 30 s, 35 yd, 5 s; Alter Time 3 min with a window up to 10 s; Pyroblast 3.5 s when not free. Derived: hold Inferno Blast, Combustion and Frostjaw for the go (healer controlled or trinketless, a Hot Streak, a freeze source, the target inside 40 yd); Deep Freeze and Combustion are stuns and share one category with the partner's stuns, so use them on a target the partner has not just stunned; Fire has no ranged snare, so kiting is Frost Nova, Blink, Cone of Cold and Ring of Frost; the Fire healer lock repeats about every 45 s, not 30 s. Extra gap lines: the go has no gate on the healer's state (`FireMageStrategy.cpp:100-101` casts Frostjaw and Combustion on cooldown), and no code reads the shared diminishing returns catalog for Deep Freeze.

**Proposals.** Config (no code): tier 45 Frostjaw (the code already casts it; guides and 3 of 4 ladder players agree); add the Deep Freeze major glyph (only useful once rule 1 exists); tier 30 stays Temporal Shield (the 5.4 guide) or moves to Flameglow (3 of 4 ladder, passive, no rule needed). Rotation (waits for the owner's in-game test): Deep Freeze window chain; Presence of Mind to sheep and Frostjaw; a Heating Up gate; healer lock chain adapted from the Frost plan.

**Open questions.** (1) Tier 30: Temporal Shield or Flameglow. (2) Is Deep Freeze wanted on a player target only. (3) Whether the Fire set should differ from Frost at all, since 22 of 23 ladder mages pooled hold Flameglow.

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 2 players of this specialisation (rating 2587 to 2669). 2v2: 2 players (rating 2718 to 2741). Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** none, only 4 players in all (noted, not changed)
- **BT-324 result:** open (pooled 4 players)
- **Set now:** `PremadeSpecLinkPvp.8.1 = 000212`, `PremadeSpecGlyphPvp.8.1 = 69773,92727,42752,42743,42735,42744`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Presence of Mind 100% / 50% / 3 of 4, Blazing Speed 0% / 50% / 1 of 4 | Presence of Mind | Presence of Mind | in the ladder, split |
| 2 (30) | Flameglow 50% / 100% / 3 of 4, Ice Barrier 50% / 0% / 1 of 4 | Temporal Shield | Temporal Shield | differs, not changed |
| 3 (45) | Frostjaw 50% / 100% / 3 of 4, Ring of Frost 50% / 0% / 1 of 4 | Ring of Frost | Ring of Frost | in the ladder, split |
| 4 (60) | Cold Snap 100% / 100% / 4 of 4 | Cold Snap | Cold Snap | matches |
| 5 (75) | Living Bomb 100% / 100% / 4 of 4 | Living Bomb | Living Bomb | matches |
| 6 (90) | Incanter's Ward 100% / 100% / 4 of 4 | Incanter's Ward | Incanter's Ward | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Polymorph (42752) | 100% | 100% | 4 of 4 | yes | yes |
| major | Deep Freeze (45740) | 100% | 100% | 4 of 4 | no | no |
| major | Ice Block (42744) | 50% | 50% | 2 of 4 | yes | yes |
| major | Armors (69773) | 50% | 50% | 2 of 4 | yes | yes |
| minor | Illusion (45738) | 50% | 100% | 3 of 4 | no | no |
| minor | Loose Mana (42735) | 0% | 0% | 0 of 4 | yes | yes |
| minor | Momentum (42743) | 0% | 0% | 0 of 4 | yes | yes |
| minor | Discreet Magic (92727) | 0% | 0% | 0 of 4 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **Open (BT-451):** four players pooled (2 + 2), one short of the minimum. Flameglow is 3 of 4 (the set holds Temporal Shield, as the old frost set did), tier 45 Frostjaw is 3 of 4 (the set holds Ring of Frost), tiers 15 (Presence of Mind 3 of 4), 60, 75 and 90 match. Fire and arcane still carry the frost link of before the ladder check (BT-324 stays open for them). Pooled with the 19 frost mages, Flameglow is held by 22 of 23 mages: a hint for the owner, not applied.
