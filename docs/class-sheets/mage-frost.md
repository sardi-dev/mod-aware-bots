# Mage: Frost (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). A Ring of Frost action
  is left for later. The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Frost Mage Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 22 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the talent hints in the text are usable).
  - wowtbc.gg, "Frost Mage PvE Guide" in the MoP class guides (MoP Classic, no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent,
    GlyphProperties, SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellRadius, SpellAuraOptions;
    `data/db2/Item-sparse.db2` for glyph item ids) through `spec_data.py` and short read-only scripts, and the core
    source (`SkyFire_548/src/server/scripts/Spells/spell_mage.cpp`, `Spell.cpp`) for the Blink glyphs.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, Icy Veins retried 2026-11-11); Wowhead needs a browser
    (its leveling page answered 404).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the
  tank's marks), 61 to 90 briefly from the guides. This sheet carries the full low level duel and kiting section (owner
  report of 2026-11-11); the Arcane and Fire sheets carry the same section for their tools.

Which source was richer: **talents (named picks, dungeon bomb rule), glyphs for dungeons, stat order (with the Haste
number), opener** Method (wowtbc lists every talent option without a pick and has no stat order); **the Fingers of Frost
rule (Ice Lance at 2 stacks first), the Water Elemental Freeze step, the defensive order, the 2 to 4 and 5 plus target
split** wowtbc; **levels 15 to 60, what to do before Ice Lance or Fingers of Frost is learned, mana, duels** neither guide
says anything (both are level 90 raid guides), so those lines come from the 5.4.8 data and are marked "derived". The two
guides agree on the single target order except for Frozen Orb: Method puts it above Frostfire Bolt on Brain Freeze and above
Ice Lance at one stack, wowtbc puts it below both. Method's order is used because Frozen Orb also grants a Fingers of Frost charge on
its first hit (spell text), and the bots cast it first in any case.

## Talents

Tiers and columns are from Talent.dbc (the three mage specialisations share one talent list). "Bots today" is
`AiPlayerbot.PremadeSpecLink.8.2 = 200000` (`conf/playerbots.conf.dist:2854`; one digit per tier, column counted from 0):
Ice Floes, Temporal Shield, Ring of Frost, Greater Invisibility, Nether Tempest, Invocation.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (200000) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Ice Floes; Presence of Mind only for fights without movement (Method) | Presence of Mind, Blazing Speed | 108839 / 12043 / 108843 | Ice Floes, same. **Dead today:** no `ice floes` action exists (`MageAiObjectContext.cpp:224-308`) |
| 2 (30) | none named for Frost (Method); wowtbc lists all three | Temporal Shield, Ice Barrier, Flameglow | 115610 / 11426 / 140468 | Temporal Shield (`GenericMageStrategy.cpp:219`); the Frost strategy also carries two Ice Barrier edges (`FrostMageStrategy.cpp:131`, `:134`) that never fire with this pick |
| 3 (45) | Ring of Frost as a situational pick; Frostjaw as the interrupt option (Method) | Ice Ward | 113724 / 102051 / 111264 | Ring of Frost. **Dead today:** no `ring of frost` action. Frostjaw has an action and is the last default (`FrostMageStrategy.cpp:106`), so it would fire only when nothing above it can be cast |
| 4 (60) | Cauterize as the safe default; Greater Invisibility to avoid raid mechanics or for skips (Method) | Cold Snap | 86949 / 110959 / 11958 | Greater Invisibility; its only edge casts `invisibility` (`GenericMageStrategy.cpp:195`); to check live whether that name still resolves once the talent replaces Invisibility |
| 5 (75) | Living Bomb on one target, Nether Tempest on 2 to 4, Frost Bomb on large packs only (Method); dungeon: Frost Bomb only for big pulls, else Nether Tempest | | 44457 / 114923 / 112948 | Nether Tempest via the Frost Bomb nodes' fallback (`FrostMageStrategy.cpp:82-96`, edge `:156`, area `:162`) |
| 6 (90) | Invocation: Evocation before the pull starts it (Method opener) | Rune of Power, Incanter's Ward | 114003 / 116011 / 1463 | Invocation, same |

Notes on the picks (data, not guide):

- Tiers 1 to 4 are the ones that matter for levels 15 to 60. Ice Floes, Ring of Frost and a Greater Invisibility cast that
  may not resolve are dead today; only Temporal Shield works. **Cauterize** (86949) is passive ("an attack which would
  otherwise kill you brings you to a part of your health", spell text), needs no code and is Method's safe default for
  Frost: the better tier 60 pick until the Invisibility action is checked (derived).
- Tier 30 for Frost is unspecified in Method, so the choice is derived: **Ice Barrier** (11426) absorbs damage for 60 s and
  "spellcasting will not be delayed by damage" (spell text), which protects every 2 s Frostbolt cast of a leveling mage that a
  melee enemy is hitting, and the Frost strategy already has the edges for it. Temporal Shield (115610) lasts 4 s. Owner
  question 3.
- Ice Floes (108839): cast while moving for the next Mage spell with a cast time under 4 s, up to 3 charges, 15 s. Frost
  already has instants for moving (Ice Lance, `FrostMageStrategy.cpp:114`), so the loss from a dead pick is smaller than for
  Arcane (derived).
- Blazing Speed (108843): speed up for 1.5 s, ignores slows, off the global cooldown, 25 s. It has an action and an edge
  (`GenericMageStrategy.cpp:225-228`) gated to battlegrounds, arenas and started duels.
- Invocation (114003): Evocation has no cooldown and a shorter channel, and a finished Evocation raises spell damage while
  lowering mana regeneration (spell text). Both guides use Evocation to keep that buff up; the bots cast Evocation only at low
  mana (`GenericMageStrategy.cpp:196`), so for them Invocation is a cheaper emergency Evocation (derived). Level 90 only.
- Nether Tempest, Living Bomb and Frost Bomb are three choices of one talent row; the spell rows say level 49 or 60 but
  they are learned with the talent at 75. Brain Freeze (77) comes only from the most recently applied of the three (spell
  text); the bots' `frost bomb` nodes fall back to the other two for that reason (`FrostMageStrategy.cpp:82-96`, comment #790 at `:70-81`).

Proposed link, guide aligned and safe for the code that exists: `220100` (Ice Floes / Ice Barrier / Ring of Frost / Cauterize
/ Nether Tempest / Invocation); the tier 30 and tier 60 digits change (owner questions 2 and 3). For a character below 75
only the first four digits are in effect.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and
75 (config comment `conf/playerbots.conf.dist:2631-2636`; the unlock levels are core-side). The config order is major,
minor, major, minor, minor, major; a glyph goes to the first free slot of its type. So the first major and first minor are
the only ones in effect from 25 to 49, the second pair from 50 to 74.

| Type | Glyph | Item id | Why (one clause) | Bots today (`42753,45739,42745,42743,42735,45736`) |
|------|-------|--------:|------------------|------------------------|
| Major | Icy Veins | 42753 | Method and wowtbc best set; the glyph turns Icy Veins' haste into Frostbolt, Frostfire Bolt and Ice Lance splitting into 3 bolts (spell text), learned at 36 | yes, in **slot 1 (level 25) where it does nothing until 36** |
| Major | Splitting Ice | 42745 | Method and wowtbc best set; Ice Lance hits one more target for 50 percent (effect value), Ice Lance is learned at 22 | yes (slot 2, level 50) |
| Major | Water Elemental | 45736 | wowtbc standard; +40 percent health and the elemental can cast while moving (effect value 40) | yes (slot 3, level 75) |
| Major, situational | Cone of Cold | 42746 or 62764 | Method dungeon best (preferred over Water Elemental); +damage on a spell the AoE strategy does cast for Frost | no. Two items carry this name in the item data; which one teaches the 5.4.8 glyph (115705) is to check in game |
| Major, situational | Rapid Displacement | 42748 | both guides, mobility; Blink gets 2 charges but no longer frees from stuns | no |
| Major, derived | Frost Nova | 42741 | the bots cast Nova every time an enemy is within 5 yd; 5 s less cooldown (effect value -5000) | no (in neither guide) |
| Major, situational | Remove Curse | 44920 | Method, situational; the bots' `cure` strategy removes curses | no |
| Minor | Momentum | 42743 | Method's only minor; Blink goes in the movement direction instead of the facing | yes (slot 2, level 50) |
| Minor | Mirror Image | 45739 | wowtbc; Mirror Images cast Frostbolt for Frost (the default), so the glyph does nothing for this specialisation | yes, in **slot 1 (level 25) where it does nothing, and never does anything for Frost** (text: "cast Arcane Blast or Fireball instead of Frostbolt depending on your primary talent tree") |
| Minor | Loose Mana | 42735 | not in a guide; Mana Gem restores over time instead of at once | yes (slot 3, level 75) |

Facts checked in the core: with Glyph of Momentum, Blink picks the movement direction (`Spell.cpp:1489-1510`); a stationary
caster keeps its facing, so the bots' `blink back` (face away, then Blink: `MageActions.cpp:38-46`) still works while
standing. Rapid Displacement blocks Blink while stunned (`spell_mage.cpp:3106-3140`).

Disagreement: Method's raid majors are Icy Veins, Splitting Ice, Water Elemental and its dungeon majors Icy Veins, Splitting
Ice, Cone of Cold; wowtbc has the raid three plus Cone of Cold for AoE and Rapid Displacement for mobility. The minor is
Momentum (Method) or Mirror Image (wowtbc): the wowtbc minor does nothing for Frost (the glyph changes nothing when the
images already cast Frostbolt), so Method's Momentum is used first. Neither guide has a second useful minor; the premade's
Mirror Image stays in the second minor slot as a harmless filler. The slot order is derived: a glyph in a slot must do
something at the level the slot unlocks. Proposed `PremadeSpecGlyph.8.2`: `42745,42743,42753,45739,42735,45736` = Splitting Ice,
Momentum, Icy Veins, Mirror Image, Loose Mana, Water Elemental (the same six glyphs; Splitting Ice moves to slot 1 at 25, where
Ice Lance exists, and Icy Veins to slot 2 at 50, after the spell at 36).

## Stat order

Method: Intellect, then Hit and Expertise to 15 percent chance to hit, then Haste (a total of 14242 rating at level 90, do not
overcap), then Mastery, then Critical Strike (low value). wowtbc has no stat order. The caps are level 90 numbers and cannot
be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Intellect first.** It is the primary stat of the class and the only way an item adds damage in 5.x; Spell Power is not an
  item stat any more (inference: the data here does not list item stats).
- Then Haste, then Hit (matters near 90), then Mastery, then Critical Strike last. Below level 80 Mastery does nothing for a
  Frost mage because Mastery: Icicles is learned at 80 (SpecializationSpells); rank it last until then. Critical strike has a
  second use at low level: Shatter (16) raises the crit chance of every spell against frozen targets (spell text), but a
  frozen target is the exception, so the order stays.
- Spirit is in neither guide. It only adds mana regeneration for a mage (inference); rank it at the bottom.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:527-539`, shared with warlock and shadow priest): Spell Power 1.0, Hit
  1.1, Haste 1.0, Mastery 0.87, Critical 0.8, **Spirit 0.6, Intellect 0.3**. The secondary order matches Method except Hit
  above Haste (harmless below 90), but **Spirit outranks Intellect two to one**, so a cloth item with Spirit beats one with
  Intellect (read). The Spirit penalty of 0.6 applies only until Molten Armor (level 34) is known (`:712-720`); the rule
  came from a Wrath of the Lich King armor that turned Spirit into critical strike, which Molten Armor no longer does in
  5.4.8.
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
| 10 | Summon Water Elemental (31687, 3 percent, 1.5 s cast, 60 s); the specialisation is chosen here |
| 12 | Frostbolt (116, 2 s cast, 4 percent, **slows 50 percent for 15 s**) |
| 14 | Polymorph (118, 1.7 s cast, 30 yd, up to 50 s) |
| 16 | Shatter (12982, passive: the crit chance of all your spells against frozen targets doubles, plus 50 percent more) |
| 18 | Arcane Explosion (1449, instant, 3 percent, 10 yd; every mage has it) |
| 22 | Ice Lance (30455, instant, 1 percent, 40 yd; **damage quadrupled against frozen targets**) |
| 24 | Fingers of Frost (112965 / aura 44544: Frostbolt and Frostfire Bolt hits have a 15 percent chance, Blizzard ticks 5 percent, to grant a charge, up to 2 for 15 s; the next Ice Lance or Deep Freeze acts as if the target were frozen and Ice Lance deals 25 percent more) |
| 26 | Ice Block (45438, 10 s, 5 min, Hypothermia afterwards) |
| 28 | Cone of Cold (120, instant, 4 percent, 10 s, slows 60 percent for 6 s; every mage has it) |
| 29 | Remove Curse (475, 8 s) |
| 32 | Slow Fall (130) |
| 34 | Molten Armor (30482, 3 s cast, critical strike up and physical damage taken down) |
| 36 | Icy Veins (12472, 3 min, 20 s: spell haste +20 percent and pushback from damage while casting removed; not on the global cooldown) |
| 38 | Conjure Refreshment (42955, 3 s cast) |
| 40 | Evocation (12051, 120 s: 15 percent of mana at once and another 45 percent over the 6 s channel, spell text) |
| 44 | Flamestrike (2120, 2 s cast, 6 percent, 8 yd) |
| 47 | Conjure Mana Gem (759, 3 s cast, 18 percent; makes item 36799) |
| 49 | Mirror Image (55342, 3 min, 30 s) |
| 50 | Wizardry (89744, +5 percent Intellect) |
| 52 | Blizzard (10, 5 percent, 8 s channel, 8 yd, slows 50 percent, may grant Fingers of Frost) |
| 54 | Frost Armor (7302, spell haste up, attackers slowed) |
| 56 | Invisibility (66, 5 min) |
| 58 | Arcane Brilliance (1459, party-wide spell power and critical strike) |
| 60 | tier 4 talent |
| 62 | Frozen Orb (84714, instant, 10 percent, 60 s, 10 s: slows 50 percent, grants a Fingers of Frost charge on its first hit) |
| 64 | Spellsteal (30449) |
| 66 | Deep Freeze (44572, 30 s, 35 yd, **stun for 5 s on a frozen target, no damage effect in the data**) |
| 70 | Improved Counterspell (12598: adds a silence) |
| 75 | tier 5 talent (bombs) |
| 77 | Brain Freeze (44549: the most recent bomb may grant it; the next Frostfire Bolt is instant, free and counts as frozen) |
| 80 | Mastery: Icicles (76613); Mage Armor (6117) |
| 84 | Time Warp (80353, 5 min) |
| 87 | Alter Time (108978, 3 min) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and data tables):

- A target is "frozen" while Frost Nova's root, Deep Freeze, the Water Elemental's Freeze or Ring of Frost holds it (inference
  from the texts; the core's own frozen test is `Unit::isFrozen`, used in `RangeTriggers.cpp:121-125`). Ice Lance does four
  times the damage against it, and Shatter doubles the crit chance of every spell.
- Fingers of Frost gives the same effect without a freeze, for two charges: Ice Lance with 2 charges first (wowtbc, Method),
  so no charge expires (they last 15 s).
- Frostbolt's chill (50 percent for 15 s) is the class's built-in kite tool; Frostfire Bolt chills 40 percent for 8 s.
- Deep Freeze does no damage in this data (spell text and effect list: a stun only); casting it on a Fingers of Frost
  charge costs the 25 percent boosted Ice Lance for a 5 s stun. Method's and wowtbc's single target lists do not contain
  it.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` at commit `5927134`.

How the bots rank what they do (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, ACTION_NORMAL 10, ACTION_HIGH 20,
ACTION_MOVE 30, ACTION_INTERRUPT 40, ACTION_EMERGENCY 90. A trigger edge adds its action with that number; the engine takes
the highest action that is possible and useful; an action whose spell is unknown is simply skipped. A Frost mage gets the
`frost` and `frost aoe` combat strategies plus `dps`, `dps assist`, `cure`, `cc` and `aoe` (`Factory/AiFactory.cpp:353-366`),
and every bot outside battlegrounds gets `boost`; non-combat it gets `bmana` (the armor strategy) and `cure`
(`AiFactory.cpp:722-729`). `BoostTrigger` (`GenericTriggers.cpp:416-424`) is active at once against a player, otherwise only
when the group is out-matched 2 to 1 (balance 50 or less; inference from the rogue sheet, to check live), so a normal 5-player
pull never gets the Mirror Image edge in `boost`; the Icy Veins edge in the Frost strategy is not gated.

### Levels 15 to 29

Known: Frostfire Bolt, Frost Nova, Fire Blast, Blink, Counterspell, Water Elemental (10), Frostbolt (12), Polymorph (14),
Shatter (16), Arcane Explosion (18), Ice Lance (22), Fingers of Frost (24), Ice Block (26), Cone of Cold (28), Remove Curse (29).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | no Water Elemental | Summon Water Elemental (31687, 10) | yes, `no pet` -> ACTION_HIGH (`FrostMageStrategy.cpp:128`); `has pet` turns on the elemental's autocast spells, Freeze among them (`:129`, `strategy/actions/GenericActions.cpp:21-61`) |
| 2 | core | filler | Frostbolt (116, 12) | yes, default 5.9 (`:111`) |
| 3 | core | Fingers of Frost charge, from 24 | Ice Lance (30455, 22), 2 charges first | yes, 18 on `fingers of frost` and 11 to 12 on the last charge (`:138-139`, `:151`), but Deep Freeze is listed before it at 19 and 12 (66 and up) |
| 4 | core | moving | Ice Lance (instant) | yes, default 5.5 (`:114`) |
| 5 | good | the target is frozen (Frost Nova just landed), from 22 | **Ice Lance** (quadruple damage), not Frostbolt | **wrong order:** `frost nova on target` -> Deep Freeze 19, then **Frostbolt 18** (`:153`); Frostbolt is a 2 s cast that may end the root and wastes the frozen state (derived; the comment at `:141-150` fixed the same mistake for Fingers of Frost only) |
| 6 | good | an enemy within 5 yd | Frost Nova (122, 3), then step away | yes, 50 (`GenericMageStrategy.cpp:210-211`) |
| 7 | good | the enemy is frozen and close | Blink (1953, 7) away, then resume | yes, `blink back` 35 (`:212-213`) |
| 8 | good | a target casts, from 8 | Counterspell (2139, 8) | **missing:** only an enemy healer (`:187`) and, in a battleground, arena or started duel, any enemy caster (`:221-222`); the plain `counterspell` trigger is registered (`MageAiObjectContext.cpp:108`) but no strategy schedules it |
| 9 | good | a second attacker or a marked target, from 14 | Polymorph (118, 14) | yes, `cc` 30 (`:284`; BT-028, live check pending) |
| 10 | good | 3 or more enemies within 8 yd of the target, from 18 | Arcane Explosion (1449, 18), 10 yd | **missing:** the Frost AoE edges start at 2 enemies for Cone of Cold (28), then Flamestrike (44) and Blizzard (52) (`FrostMageStrategy.cpp:160-170`, `GenericMageStrategy.cpp:316-322`) |
| 11 | good | 2 or more enemies in melee reach, from 28 | Cone of Cold (120, 28) | yes: `light aoe` 23 and 21 (`FrostMageStrategy.cpp:168`, `GenericMageStrategy.cpp:321`), only while the target is within 10 yd and in front (`MageActions.cpp:63-68`) |
| 12 | expert | health at or under 25 percent | Ice Block (45438, 26) | yes, 90 (`GenericMageStrategy.cpp:193`); in a started duel only in a group at 10 percent (`MageTriggers.cpp:192-199`) |
| 13 | good | a party member is cursed | Remove Curse (475, 29) | yes, `cure` 41 and 40 (`GenericMageStrategy.cpp:243-247`) |

Before a spell is learned (derived): below 10 there is no specialisation and the kit is Frostfire Bolt, Frost Nova, Fire
Blast, Blink and Counterspell; the bot gets the `frost` strategy then (`AiFactory.cpp:280-287`), whose Frostfire Bolt is the
only filler. From 10 to 11 the Frost mage has the Water Elemental and Frostfire Bolt; Frostbolt waits for 12.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 14 | core | out of combat, armor missing | Molten Armor (30482, 34) until Frost Armor at 54 (derived: the only armor known) | **solo only from 54:** `FrostArmorTrigger` wants Frost Armor when the mage is alone (`MageTriggers.cpp:42-47`), `MageArmorTrigger` wants Mage Armor (learned at 80) in a group (`:34-40`), `MoltenArmorTrigger` is Fire only (`:49-52`); a grouped Frost mage casts no armor until 80 (read) |
| 15 | core | always | Icy Veins (12472, 36) on cooldown (both guides) | yes, 50 (`FrostMageStrategy.cpp:121`), not gated by the balance rule, also 27.5 in `boost` |
| 16 | core | low mana | Evocation (12051, 40), then a mana potion | yes, 90 (`GenericMageStrategy.cpp:196`) |
| 17 | good | no conjured food in the bags, from 38 | Conjure Refreshment (42955, 38) | yes (`GenericMageNonCombatStrategy.cpp:62`, BT-029) |
| 18 | good | 3 or more enemies within 8 yd | Flamestrike (2120, 44), then Blizzard | yes, 23 and 22 (`GenericMageStrategy.cpp:316-322`); Flamestrike is a Fire spell in the guides, every mage has it from the class list |
| 19 | good | Fingers of Frost charge on a Frozen target | do not use Deep Freeze (66) on it | see line 3 |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | good | mana at the "high mana" threshold, gem in the bags, from 47 | Conjure Mana Gem (759, 47), then use the gem | yes, but only if the spell is known when the strategy is built (`GenericMageStrategy.cpp:239-240`; inference, to check) |
| 21 | good | on a pack of elites or a boss, from 49 | Mirror Image (55342, 49) on cooldown, with Icy Veins (both guides) | partly: `boost` 26 (`GenericMageStrategy.cpp:278`), gated by the balance rule above |
| 22 | good | 3 or more enemies, from 52 | Blizzard (10, 52), cancelled when fewer than 2 enemies are left | yes (`GenericMageStrategy.cpp:289`) |
| 23 | core | out of combat, armor missing | Frost Armor (7302, 54): spell haste (both guides: "maintain Frost Armor") | solo only (line 14) |
| 24 | good | two attackers, from 56 | Invisibility (66, 56) as a threat dump (derived) | yes, 30 (`:195`, `MageTriggers.cpp:202-209`); a 5 minute cooldown |
| 25 | core | out of combat, party buff missing, from 58 | Arcane Brilliance (1459, 58) | yes, 21 (`GenericMageNonCombatStrategy.cpp:55`) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 26 | core | cooldown ready, enemy 30 yd away and in front | Frozen Orb (84714, 62) on cooldown | yes, the first default (5.95), range gate 30 yd (`FrostMageStrategy.cpp:110`, `MageActions.cpp:70-76`) |
| 27 | good | Fingers of Frost charges | Ice Lance first at 2 charges, then Brain Freeze's Frostfire Bolt, then Ice Lance at 1 (wowtbc) | yes in outline: any charge 18 (`:151`), last charge 11 to 12 (`:138`), Brain Freeze 13 (`:130`); Deep Freeze on top of it (66) |
| 28 | good | casters in range | Counterspell with silence (Improved Counterspell, 70) | missing, see line 8 |
| 29 | good | tier 4 talent | Cauterize passive, or Greater Invisibility | see Talents |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | Living Bomb on one target, Nether Tempest on 2 to 4, Frost Bomb on large packs (both guides) | `frost bomb` edge 19 with the Nether Tempest then Living Bomb fallback (`FrostMageStrategy.cpp:156`, `:82-96`); on attackers in the AoE strategy 24 (`:162`, `:167`) |
| 77 | Brain Freeze: Frostfire Bolt on the proc | yes, 13 (`:130`) |
| 80 | Mastery: Icicles; Mage Armor is not in either guide (Frost Armor is) | a grouped Frost mage wears Mage Armor (`MageTriggers.cpp:34-40`): differs from both guides |
| 84 | Time Warp on a big pull | missing (no action) |
| 87 | Alter Time during Icy Veins | missing (no action) |
| 90 | single target (wowtbc): Frost Armor, Mirror Image, Icy Veins, Alter Time, Evocation for Invocation, Ice Lance at 2 charges, Freeze, Living Bomb, Frostfire Bolt on Brain Freeze, Ice Lance at 1, Frozen Orb, Frostbolt; area: Nether Tempest or Frost Bomb, Frozen Orb, Cone of Cold, Arcane Explosion over 4 targets | Icy Veins, Mirror Image, Ice Lance, Brain Freeze, Frozen Orb, Frostbolt, the bomb are there; Frost Armor (grouped), Alter Time, Evocation for Invocation, Arcane Explosion are not |

## Cooldowns and defensives

The guides say "on cooldown"; the timing for a dungeon bot is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Icy Veins | 36 (3 min) | on cooldown; Alter Time during it | at a pack of elites or a boss | on every ready cooldown, 50 (`FrostMageStrategy.cpp:121`) |
| Mirror Image | 49 (3 min) | on cooldown, with Icy Veins | at a pack of elites or a boss | `boost` 26, balance rule (`GenericMageStrategy.cpp:278`) |
| Frozen Orb | 62 (60 s) | on cooldown | when enemies are in front within 30 yd | first default (`FrostMageStrategy.cpp:110`) |
| Evocation | 40 (120 s) | before the pull and to keep Invocation | low mana, when the tank holds the enemies | at low mana, 90 (`:196`) |
| Frost Nova / Blink | 3 / 7 | not covered | Nova a melee enemy in reach, Blink out | `GenericMageStrategy.cpp:210-213` |
| Ice Block | 26 (5 min) | first in wowtbc's defensive order | health under 25 to 35 percent with nothing else ready | 25 percent (`:193`); solo duel never |
| Temporal Shield / Ice Barrier | talent 30 | wowtbc: third / not named | when attacked | Temporal Shield 28.5 (`:219`); Ice Barrier 29 and `medium health` only if picked (`FrostMageStrategy.cpp:131-134`) |
| Cauterize / Greater Invisibility | talent 60 | Method: Cauterize as the safe default / Greater Invisibility | death / threat dump | see Talents |
| Counterspell | 8 (24 s) | not covered | any enemy cast of a dangerous spell | healers and BG/duel casters only |
| Cold Snap | talent 60 | resets Ice Block, Frost Nova, Cone of Cold only (Method, Arcane page) | n/a for the premade | WotLK logic: `ColdSnapTrigger` waits for Icy Veins and Deep Freeze on cooldown (`MageTriggers.h:256-260`), which Cold Snap does not reset in 5.4.8 |
| Deep Freeze | 66 (30 s) | not in either list | PvP stun only | on every Fingers of Frost charge and every Nova (`FrostMageStrategy.cpp:138`, `:151`, `:153`) |
| Water Elemental Freeze | 10 | wowtbc: on rootable adds | automatic | autocast on (`GenericActions.cpp:21-61`) |

## Healer triage and mana (healers only)

Not applicable to healing. Mana: Frostbolt costs 4 percent per 2 s and Ice Lance 1 percent, so Frost is the cheapest of the
three damage trees per second of damage (derived from the costs); Evocation arrives at 40 and the Mana Gem at 47, so below
40 a long fight ends in a drink. The mana lines the bots have are the same as for the other mages: Evocation at 15 percent
(`GenericMageStrategy.cpp:196`), the Mana Gem, the dungeon drink rule (`GenericTriggers.cpp:50-60`, below 50 percent between
pulls, config `AiPlayerbot.DungeonDrinkMana`) and the generic drink at `LowMana` 15, `MediumMana` 40
(`conf/playerbots.conf.dist:674-675`).

## Low level duels and kiting (owner report 2026-11-11)

What the guides say: nothing. Both are level 90 PvE guides. The only related lines: Method (Arcane page) says Cold Snap resets
Ice Block, Frost Nova and Cone of Cold only; wowtbc's defensive order for Frost is Ice Block, Greater Invisibility, Temporal
Shield; Method's Frost page offers Frostjaw as an interrupt and Cauterize as the safe default. Frostbolt's chill, the timing
of Frost Nova, Blink and Ice Block at low levels are not in either guide, so everything below is derived from the 5.4.8
spell data and marked as such.

What a Frost mage has, by level (data):

| Level | Tool | What it does |
|------:|------|--------------|
| 1 | Frostfire Bolt | 2.75 s cast, chills 40 percent for 8 s |
| 3 | Frost Nova | roots everything within 12 yd for up to 8 s, "damage may interrupt", 25 s cooldown (20 s with the Frost Nova glyph) |
| 5 | Fire Blast | instant, 30 yd, 8 s |
| 7 | Blink | 20 yd forward (more with the Blink glyph), 15 s, frees from stuns and bonds |
| 8 | Counterspell | 6 s lock of the school, 24 s, 40 yd, no global cooldown |
| 12 | Frostbolt | 2 s cast, **chills 50 percent for 15 s** (a bolt every 2 s keeps it up) |
| 14 | Polymorph | 1.7 s cast, 30 yd, the enemy cannot attack until damaged |
| 16 / 22 | Shatter / Ice Lance | crit chance doubled and Ice Lance quadrupled against a frozen enemy: Nova then Ice Lance is the burst |
| 26 | Ice Block | 10 s immune, cannot act, 5 min cooldown, Hypothermia afterwards |
| 28 | Cone of Cold | instant, chills 60 percent for 6 s, 10 s cooldown |
| 36 | Icy Veins | +20 percent haste, no pushback for 20 s |
| 62 | Frozen Orb | chills 50 percent in a line, grants Fingers of Frost |
| 66 | Deep Freeze | 5 s stun on a frozen target |
| talents | Ice Barrier (30), Ring of Frost, Frostjaw (45), Cold Snap, Greater Invisibility, Cauterize (60), Blazing Speed (15) | see Talents |

Sequence for a duel against a melee opponent (derived, with the reason):

1. From the first cast, keep the chill up (Frostbolt from 12, Frostfire Bolt before): a chilled melee enemy at 50 percent
   speed needs twice as long to reach the mage, which is what the kite rule of BT-145 measures (speed under 85 percent of the
   bot's, `RangeTriggers.cpp:63-119`).
2. When the enemy is about 10 yd away and not yet rooted, cast Frost Nova (radius 12 yd): the root then lands before the first
   hit, and the mage has 8 s. The bot waits for 5 yd.
3. After Nova, put distance in before doing damage: Blink 20 yd away from the enemy (15 s cooldown), or run if Blink is
   down. A spell that hits the rooted enemy may end the root, so no Frostbolt
   while the enemy is still in reach; **Ice Lance** is the exception when the mage is already out of reach, because it
   does four times damage on a frozen target.
4. Resume the chill from range. Cone of Cold (28) re-chills at 60 percent when the enemy reaches melee after the Nova ended;
   Counterspell the enemy's heal or cast (24 s cooldown).
5. Ice Block only when Nova and Blink are on cooldown and health is low (about 30 to 35 percent). Ten seconds of immunity is
   shorter than Blink (15 s) and Nova (25 s), so the block alone does not end the fight; Cold Snap (talent, resets Ice Block, Nova
   and Cone of Cold) or the Ice Block glyph (a free Nova when the block ends) turn it into an escape.
6. Polymorph is an expert tool in 1 against 1: a polymorphed enemy cannot attack for up to 8 s on a player (the data gives 50 s
   base; the length on players is to check), which is a free gap to Blink or to drink.

What the bots do (read): in a started duel `in pvp instance` is true (`PvpRotationValues.cpp:189-195`), so the lane
`mage pvp melee enemy player` is active: Frost Nova 50, Blink back 35, Blazing Speed 34 when the enemy player is in melee
range (`GenericMageStrategy.cpp:225-228`), Counterspell on an enemy caster (`:221-222`), and the flee edge is off
(`MageTriggers.cpp:211-218`). `enemy is close` -> Nova 50 and `enemy too close for spell` -> Blink back 35 apply everywhere
(`:210-213`). The kite (`PvpKiteTrigger`, `RangeTriggers.cpp:63-119`) runs only from a slowed enemy or steps out of reach of a
rooted one; for Frost the chill makes the enemy "slowed" (0.5 against 0.85), so the kite works as BT-145 intends once the
bot is at Frostbolt (12). Before 12, Frostfire Bolt chills 40 percent (0.6, also under 0.85) only while its 8 s last. Where
they fall short:

1. **Frostbolt on a rooted target** (`FrostMageStrategy.cpp:153`, `frost nova on target` -> Frostbolt 18): the bot's own
   damage may end the root, and it could have cast Ice Lance (22) for four times the damage (line 5 above). Between 12 and 21
   there is no Ice Lance; then the right action is to move first (the kite does) and not to cast at all while in reach.
   Whether a Frostbolt ends the root in this core is to check live.
2. **Deep Freeze on every Fingers of Frost charge** (66 and up; `FrostMageStrategy.cpp:138`, `:151`, `:153`): a stun, no
   damage, in place of the boosted Ice Lance. A good PvP stun, a waste in a dungeon where bosses resist it.
3. The Nova trigger distance is the generic `TooCloseDistance` 5 yd (`conf/playerbots.conf.dist:646`) although Nova's radius
   is 12 yd and `CastFrostNovaAction::isUseful` allows 10 (`MageActions.cpp:24-28`): the enemy is already hitting the bot
   when the root lands, and Blink back needs the enemy within `MIN_MELEE_REACH` of the bot too (`RangeTriggers.cpp:121-125`).
4. A solo duelist never casts Ice Block: the duel counts as a pvp instance and the rule there is "in a group and health at or
   under 10 percent" (`MageTriggers.cpp:192-199`); everywhere else it is 25 percent. Cold Snap and the Ice Block glyph are
   not used (and not picked).
5. No Ice Barrier (Temporal Shield is picked): a melee enemy delays every Frostbolt; Ice Barrier removes the pushback.
6. Cone of Cold is cast only by the Frost AoE strategy, which needs two enemies (`GenericMageStrategy.cpp:321`,
   `FrostMageStrategy.cpp:168`); against one enemy player in melee it is never used, although it is the second chill.
7. Counterspell on the duel opponent exists (BG/duel lane) but not in plain world PvP or PvE.
8. No Polymorph in 1 against 1.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Time Warp (84), Alter Time (87), the tier 5 bombs (75), Brain Freeze (77) and the tier 6 talents
  (90) are not available in the 15 to 60 band; Icy Veins is learned at 36, Frozen Orb at 62, Mirror Image at 49, Arcane
  Brilliance at 58, Frost Armor at 54. The lists above start without them.
- Both guides open with "Frost Armor"; it is learned at 54 here, Molten Armor at 34, and Mage Armor (80) is in neither guide.
  The bots' grouped Frost armor edge is Mage Armor (BT-027).
- Both guides' list has no Deep Freeze: it is a stun without damage in this data, so it is a PvP tool; the bots cast it on
  every Fingers of Frost charge.
- Fingers of Frost: "Limit 2 charges" (aura 44544, cumulative 2); Frostbolt does not consume it (confirmed by the #790 comment
  at `FrostMageStrategy.cpp:141-150`); the old Wrath rule "the next two spells are frozen" is gone.
- Wowtbc's minor glyph Mirror Image does nothing for Frost (its text names Arcane Blast and Fireball as replacements).
- Spell Power is not an item stat in 5.x (inference); the bots' weight list still carries Spirit above Intellect.
- Water Elemental: both guides name only the glyph; the elemental itself is automatic here (`FrostMageStrategy.cpp:128-129`).

## Gap list (ordered)

Config (can go at once):

1. Glyph order and replacement: `PremadeSpecGlyph.8.2` (`conf/playerbots.conf.dist:2856`) to `42745,42743,42753,45739,42735,45736`.
   Today a level 25 to 49 Frost bot carries Icy Veins (nothing before 36) and Mirror Image (nothing before 49, and nothing for
   Frost afterwards) in its first slots.
2. Tier 60: `Cauterize` (digit 1) and, at tier 30, Ice Barrier (digit 2): `PremadeSpecLink.8.2` (`:2854`) to `220100`.
   Reason: a passive cheat death needs no code and the Invisibility cast is unverified; Ice Barrier already has its edges
   (`FrostMageStrategy.cpp:131-134`). Owner question.
3. Tiers 15 and 45 are dead talents (Ice Floes, Ring of Frost): keep the guide picks only together with rotation item 9;
   otherwise Blazing Speed (digit 1 at tier 15) is the one with code.

Rotation (class fixes, wait for the owner's in-game test):

1. Frost armor (core): a grouped Frost mage wears nothing between 34 and 79, and Mage Armor from 80 against both guides'
   Frost Armor. Fix `MageTriggers.cpp:34-52` (`MageArmorTrigger`, `FrostArmorTrigger`, `MoltenArmorTrigger`) and the nodes in
   `GenericMageNonCombatStrategy.cpp:67-79`: Frost and Arcane use Frost Armor when known, else Molten Armor; Mage Armor only
   when neither is known. To check live: BT-027 is deployed and its live check is pending.
2. `frost nova on target` (good tier): replace Frostbolt with Ice Lance (22 and up) and, below 22, no
   cast while the enemy is in reach (the kite moves the bot): `FrostMageStrategy.cpp:153`.
3. Deep Freeze (good): drop it from the Fingers of Frost and Nova edges (`FrostMageStrategy.cpp:138`, `:151`, `:153`) in PvE and
   keep it for a player target; the `fingers of frost` edge then reads Ice Lance only, with Ice Lance at 2 charges first as
   wowtbc has it (`:138-139` already handles the last charge).
4. Counterspell on the current target's cast (good): add `TriggerNode("counterspell", ...)` at 40 to
   `GenericMageStrategy::InitTriggers` (`:187`), using the registered `CounterspellInterruptSpellTrigger`
   (`MageTriggers.h:62-66`). Applies to all three specialisations.
5. Cone of Cold as a single target chill in PvP (28): an edge `enemy player in melee range` and the target not chilled, in
   `FrostMageStrategy::InitTriggers`; the action already checks facing and 10 yd (`MageActions.cpp:63-68`).
6. Duel Ice Block: `MageIceBlockHealthTrigger` (`MageTriggers.cpp:192-199`): a solo duelist never casts it. A rule for a
   started duel: health at or under 30 percent with Nova and Blink on cooldown. Nova distance for an enemy player 8 to 10 yd
   (PvP only), `GenericMageStrategy.cpp:210-211` or a new `mage pvp` trigger beside `:225-228`.
7. Arcane Explosion as the AoE filler for 3 or more enemies within 10 yd from level 18 until Flamestrike (44) and Blizzard
   (52) take over: `MageAoeStrategy` (`GenericMageStrategy.cpp:316-322`). Guard against Polymorphed marked targets in the area
   (to check).
8. Mirror Image on a pack of elites or a boss (good): `MageBoostStrategy` (`GenericMageStrategy.cpp:278`) uses the balance
   gate; add a rule for 3 or more attackers or a boss in addition to "outmatched 2 to 1". Icy Veins (`FrostMageStrategy.cpp:121`)
   is ungated and fires on trash: gate it the same way for its 20 s window (derived). Good tier.
9. Ice Floes action (self buff, before a cast that has to be made while moving) and the `ring of frost` action
   (ground-targeted at the pack) if the talents stay picked; both are expert-level extras. Alter Time and Time Warp are
   outside this band.
10. Stat weights (gear scoring): Intellect above Spirit and Spirit to 0 for the mage branch
    (`StatsWeightCalculator.cpp:527-539`, `:712-720`), Mastery last below level 80. Applies to all three mage sheets.
11. `ColdSnapTrigger` (`MageTriggers.h:256-260`) waits for Icy Veins and Deep Freeze on cooldown, a WotLK rule; Cold Snap
    now resets Ice Block, Frost Nova and Cone of Cold. Only relevant if Cold Snap is ever picked.
12. Skill tiers: the mage strategies have no tier gating (`BotSkill` is used only in the tank code). Lines marked good and
    expert above (Counterspell, Polymorph, Blink-back, the Ice Lance order, Mirror Image timing, Ice Block) are the ones a
    casual bot should drop.
13. To check live: BT-027 (armor), BT-028 (Polymorph on a second attacker) and BT-029 (Conjure Refreshment) are deployed with
    their live check pending; whether the `invisibility` action resolves with Greater Invisibility; whether a Frostbolt ends
    the Frost Nova root; whether Frost Nova in a dungeon roots the tank's enemies and breaks the mage's own Polymorph (the
    Nova deals damage; read, not tested); whether Blink back burns its 15 s cooldown on the tank's target near the mage
    (`enemy too close for spell` is true for any current target that is not attacking the bot, `RangeTriggers.cpp:121-125`;
    inference).

Owner questions: (1) Tier 15 for a bot with no Ice Floes code: keep the guide pick or Blazing Speed. (2) Tier 60: Cauterize (no
code) or Greater Invisibility (guide-listed, code unverified). (3) Tier 30 Frost: Temporal Shield (as today) or Ice Barrier
(derived, edges exist). (4) May the first glyph slots hold a glyph the guide ranks lower so that every slot works at its
level. (5) Is Deep Freeze wanted on a player target only (PvP), as proposed.

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 11 players of this specialisation (rating 2447 to 2929). 2v2: 8 players (rating 2656 to 2903). Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** 3v3 (11 players)
- **BT-324 result:** updated (BT-450); 2v2 agrees
- **Set now:** `PremadeSpecLinkPvp.8.2 = 010212` (was `000212`), `PremadeSpecGlyphPvp.8.2 = 69773,92727,42752,42743,42735,45740`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Presence of Mind 100% / 100% / 19 of 19 | Presence of Mind | Presence of Mind | matches |
| 2 (30) | Flameglow 100% / 100% / 19 of 19 | Temporal Shield | Flameglow | changed (11 of 11) |
| 3 (45) | Ring of Frost 82% / 88% / 16 of 19, Ice Ward 18% / 12% / 3 of 19 | Ring of Frost | Ring of Frost | matches |
| 4 (60) | Cold Snap 100% / 100% / 19 of 19 | Cold Snap | Cold Snap | matches |
| 5 (75) | Living Bomb 100% / 100% / 19 of 19 | Living Bomb | Living Bomb | matches |
| 6 (90) | Incanter's Ward 100% / 100% / 19 of 19 | Incanter's Ward | Incanter's Ward | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Deep Freeze (45740) | 100% | 100% | 19 of 19 | yes | yes |
| major | Armors (69773) | 100% | 100% | 19 of 19 | yes | yes |
| major | Polymorph (42752) | 73% | 88% | 15 of 19 | yes | yes |
| minor | Illusion (45738) | 36% | 38% | 7 of 19 | no | no |
| minor | Loose Mana (42735) | 0% | 0% | 0 of 19 | yes | yes |
| minor | Momentum (42743) | 0% | 0% | 0 of 19 | yes | yes |
| minor | Discreet Magic (92727) | 0% | 0% | 0 of 19 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **What changed (3v3, 11 players):** tier 30 is Flameglow (100 percent in 3v3 and in 2v2) instead of Temporal Shield; the guide this set came from also named Flameglow for 3v3. The other five tiers and the three major glyphs match in both brackets (Ring of Frost 82/88, Polymorph 73/88).
- **Effect on the bots:** Flameglow is a passive absorb with a core script (`spell_mage_flameglow`), so it needs no rule. Temporal Shield was the only tier 30 talent the bots cast (`GenericMageStrategy.cpp:222`, "being attacked"); with this set that edge finds no known spell, as with any unpicked talent (to check live that an unknown spell costs nothing).
- **Left open:** the minor slots (the set holds Discreet Magic, Momentum, Loose Mana, none of them on either ladder; Illusion is 36 and 38 percent, no majority).

## PvP class package: bots today (BT-650 to BT-656, 2026-11-19)

Status of the digest's section 12 rules after the Frost package (see `docs/pvp/class-packages.md`; all wait for the owner's in-game test):

| Digest rule | Bots today |
|---|---|
| 1 healer lock every 30 s | Deep Freeze, Polymorph and Frostjaw were links of the shared chain (BT-629); Ring of Frost is now an armed link (BT-650); Deep Freeze is kept for the healer in a team fight (BT-654). The 30 s timer is the cooldown of Deep Freeze itself |
| 2 Orb, Alter Time, dump | Alter Time in the window (BT-651), Frozen Orb held for the window (BT-653); the dump is the existing Fingers of Frost and Brain Freeze edges |
| 3 no Alter Time with Hypothermia | done (BT-651) |
| 4 Cone of Cold on a melee target | already there through `mage player slow` (the digest was stale) |
| 5 Blink for a stun | partly: the shared defensive layer (BT-522) breaks stuns by cost, Blink is not held for them yet |
| 6 Pet Freeze | done (BT-652) |
| 7 Living Bomb and a sheep | the damage hold of BT-630 blocks a cast into a sheep; the explosion itself cannot be cancelled |
| 8 Ring of Frost for a druid in form or a Stampede | not built (the healer link only) |
| 9 armor by enemy comp | not built |
| 10 Counterspell on the healer's heal | the interrupt gate (BT-521) |
| 11 Ice Block last | partly (hp rule) |
| 12 pet away from melee cleave | not built |

Set: unchanged, ladder 100 percent on five tiers and Ring of Frost 82 percent; Incanter's Ward and Presence of Mind now have rules that use them.
