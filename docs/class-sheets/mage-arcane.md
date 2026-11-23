# Mage: Arcane (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). A Ring of Frost action
  is left for later. The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Arcane Mage Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 22 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the talent hints in the text are usable).
  - wowtbc.gg, "Arcane Mage PvE Guide" in the MoP class guides (MoP Classic, no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent,
    GlyphProperties, SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellRadius, SpellAuraOptions;
    `data/db2/Item-sparse.db2` for glyph item ids) through `spec_data.py` and short read-only scripts, and the core
    source (`SkyFire_548/src/server/scripts/Spells/spell_mage.cpp`, `Spell.cpp`) for the Blink glyphs.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, Icy Veins retried 2026-11-11); Wowhead needs a browser
    (its leveling page answered 404).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the
  tank's marks), 61 to 90 briefly from the guides. A section on low level duels and kiting is included (owner report of
  2026-11-11).

Which source was richer: **talents (named picks, dungeon swaps), glyphs for dungeons, stat order, the Arcane Missiles rule
("wait for 4 charges")** Method (wowtbc lists every option without a pick and has no stat order); **the defensive order
(Ice Block, Greater Invisibility, Temporal Shield), the 2 to 4 and 5 plus target split, the explicit cast-while-moving
step** wowtbc; **levels 15 to 60, mana, what to do before a spell is learned, duels** neither guide says anything (both are
level 90 raid guides, neither has a mana or Evocation line for Arcane), so those lines come from the 5.4.8 data and are
marked "derived". The guides agree on the single target order; they differ in the minor glyph (Method: Momentum, wowtbc: Mirror Image)
and in the dungeon majors (Method only: Arcane Explosion and Cone of Cold); Method is used for the picks and wowtbc's minor is
kept as the second minor.

## Talents

Tiers and columns are from Talent.dbc (the three mage specialisations share one talent list). "Bots today" is
`AiPlayerbot.PremadeSpecLink.8.0 = 200001` (`conf/playerbots.conf.dist:2844`; one digit per tier, column counted from 0):
Ice Floes, Temporal Shield, Ring of Frost, Greater Invisibility, Nether Tempest, Rune of Power.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (200001) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Ice Floes (Method), listed beside Presence of Mind and Blazing Speed (wowtbc) | Blazing Speed, see the notes | 108839 / 12043 / 108843 | Ice Floes, same. **Dead today:** no `ice floes` action exists (`MageAiObjectContext.cpp:224-308`), so it is never pressed |
| 2 (30) | Temporal Shield (Method) | Ice Barrier, Flameglow (wowtbc options) | 115610 / 11426 / 140468 | Temporal Shield, same; edge `being attacked` -> `temporal shield` (`GenericMageStrategy.cpp:219`) |
| 3 (45) | Ring of Frost, swap by encounter (Method) | Ice Ward, Frostjaw | 113724 / 111264 / 102051 | Ring of Frost, same. **Dead today:** no `ring of frost` action |
| 4 (60) | Cauterize or Greater Invisibility, both "great" (Method); dungeon: Greater Invisibility may be better | Cold Snap | 86949 / 110959 / 11958 | Greater Invisibility; its only edge casts `invisibility` (`GenericMageStrategy.cpp:195`); to check live whether that name still resolves once the talent replaces Invisibility |
| 5 (75) | Living Bomb on one target, Nether Tempest on 2 to 4, Frost Bomb on 5 or more; dungeon: Frost Bomb only if the tank pulls big packs | the other two | 44457 / 114923 / 112948 | Nether Tempest (`ArcaneMageStrategy.cpp:90`, area `:96`), the dungeon compromise |
| 6 (90) | Rune of Power, the only option Method lists (wowtbc lists Invocation and Incanter's Ward too) | Invocation | 116011 / 114003 / 1463 | Rune of Power, same (`GenericMageStrategy.cpp:198`) |

Notes on the picks (data, not guide):

- Tiers 1 to 4 are the ones that matter for levels 15 to 60. Three of the four premade picks have no code behind them
  (Ice Floes, Ring of Frost, and a Greater Invisibility cast that may not resolve); only Temporal Shield works. Passive
  talents need no code: **Cauterize** (86949, "an attack which would otherwise kill you brings you to a part of your
  health", spell text) protects a dungeon bot without any action, which is why it is the safer tier 60 pick until the
  Invisibility action is checked (derived).
- Blazing Speed (108843): speed up for 1.5 s, ignores slows, off the global cooldown, 25 s. It has an action and an edge
  (`GenericMageStrategy.cpp:225-228`) but the edge is gated to battlegrounds, arenas and started duels. It is the only
  tier 1 talent with working code.
- Ice Floes (108839): cast while moving for the next Mage spell with a cast time under 4 s, up to 3 charges, 15 s. The
  value is highest on Arcane Blast (2 s cast) and Arcane Missiles when the bot has to move (derived).
- Nether Tempest, Living Bomb and Frost Bomb are three choices of one talent row; the spell rows say level 49 or 60 but
  they are learned with the talent at 75.
- Rune of Power replaces Evocation (spell text), so the `low mana` -> `evocation` edge (`GenericMageStrategy.cpp:196`)
  stops working at 90 with this pick; with Invocation (the Fire and Frost pick) Evocation has no cooldown.

Proposed link, guide aligned and safe for the code that exists: `200101` (Ice Floes / Temporal Shield / Ring of Frost /
Cauterize / Nether Tempest / Rune of Power); only the tier 60 digit changes (owner question 2). For a character below 75
only the first four digits are in effect.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and
75 (config comment `conf/playerbots.conf.dist:2631-2636`; the unlock levels are core-side). The config order is major,
minor, major, minor, minor, major; a glyph goes to the first free slot of its type. So the first major and first minor are
the only ones in effect from 25 to 49, the second pair from 50 to 74.

| Type | Glyph | Item id | Why (one clause) | Bots today (`44955,45739,42748,42743,42735,42738`) |
|------|-------|--------:|------------------|------------------------|
| Major | Arcane Power | 44955 | Method and wowtbc best pair; doubles the duration and the cooldown of Arcane Power (spell text), learned at 62 | yes, in **slot 1 (level 25) where it does nothing until 62** |
| Major | Rapid Displacement | 42748 | Method and wowtbc; Blink gets 2 charges but no longer frees from stuns (spell text), and Blink is learned at 7 | yes (slot 2, level 50) |
| Major | Evocation | 42738 | Method and wowtbc; health back with Evocation (with Rune of Power on completion) | yes (slot 3, level 75) |
| Major, situational | Cone of Cold | 42746 or 62764 | AoE swap (both guides, Method dungeon list); the bots never cast Cone of Cold for Arcane, see the gap list | no. Two items carry this name in the item data; which one teaches the 5.4.8 glyph (115705) is to check in game |
| Major, situational | Arcane Explosion | 42736 | Method dungeon list; bigger radius for a spell the bots do not cast | no |
| Major, derived | Frost Nova | 42741 | the bots cast Nova every time an enemy is within 5 yd; 5 s less cooldown (effect value -5000) | no (in neither guide) |
| Minor | Momentum | 42743 | Method's only minor; Blink goes in the movement direction instead of the facing | yes (slot 2, level 50) |
| Minor | Mirror Image | 45739 | wowtbc; Mirror Images cast Arcane Blast instead of Frostbolt; Mirror Image is learned at 49 | yes, in **slot 1 (level 25) where it does nothing until 49** |
| Minor | Loose Mana | 42735 | not in a guide; Mana Gem restores over time instead of at once | yes (slot 3, level 75) |

Facts checked in the core: with Glyph of Momentum, Blink picks the movement direction (`Spell.cpp:1489-1510`
`TARGET_DEST_CASTER_MOVEMENT_DIR`); a stationary caster has no movement flag and keeps its facing, so the bots'
`blink back` (face away, then Blink: `MageActions.cpp:38-46`) still works while standing; a caster that is running forward
blinks along its new facing. Rapid Displacement blocks Blink while stunned (`spell_mage.cpp:3106-3140`), which is the
price of its second charge.

Disagreement: Method's raid majors are Arcane Power, Evocation, Rapid Displacement and its dungeon majors Arcane Power,
Arcane Explosion, Cone of Cold; wowtbc has the raid three plus Cone of Cold for AoE; the minor is Momentum (Method) or Mirror
Image (wowtbc). The premade follows wowtbc for the minor and Method for the majors. The slot order is derived: a glyph in a
slot must do something at the level the slot unlocks. Proposed `PremadeSpecGlyph.8.0`:
`42748,42743,44955,45739,42735,42738` = Rapid Displacement, Momentum, Arcane Power, Mirror Image, Loose Mana, Evocation (the
same six glyphs, only the order changes: Arcane Power waits for slot 2 at 50, Mirror Image for slot 2 at 50).

## Stat order

Method: Intellect, then Hit and Expertise to 15 percent chance to hit, then a little Haste, then everything else into
Mastery; Critical Strike is not recommended. wowtbc has no stat order. The caps are level 90 numbers and cannot be
reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Intellect first.** It is the primary stat of the class and the only way an item adds damage in 5.x; Spell Power is not
  an item stat any more (inference: the data here does not list item stats).
- Then Haste, then Hit, then Mastery, then Critical Strike last. Below level 80 Mastery does nothing for an Arcane mage
  because Mastery: Mana Adept is learned at 80 (SpecializationSpells); rank it last until then.
- Spirit is in neither guide. It only adds mana regeneration for a mage (inference); rank it at the bottom.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:527-539`, shared with warlock and shadow priest): Spell Power 1.0, Hit
  1.1, Haste 1.0, Mastery 0.87, Critical 0.8, **Spirit 0.6, Intellect 0.3**. The secondary order differs from the guide
  only in putting Hit above Haste (harmless below 90), but **Spirit outranks Intellect two to one**, so a cloth item with
  Spirit beats one with Intellect (read). The Spirit penalty of 0.6 applies only until Molten Armor (level 34) is known
  (`:712-720`); the comment there says the rule came from a Wrath of the Lich King armor that turned Spirit into critical
  strike, which Molten Armor no longer does in 5.4.8 (spell text: critical strike chance and less physical damage taken).
  Arcane also has an Arcane Focus hit bonus rule (`:939`) for a talent that is not in 5.4.8 (inference, to check).
- Weapons: a staff or a one-hander with an off-hand; the main-hand weight of a caster is cut to 0.65 (`:851-855`), which
  is fine because a caster cannot dual wield.

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts; the data holds old rows of the same name. Costs are percent of base mana
(SpellPower). The global cooldown is 1.5 s unless noted.

| Level | Spell (id) |
|------:|------------|
| 1 | Frostfire Bolt (44614, 2.75 s cast, 4 percent, slows 40 percent for 8 s; every mage has it) |
| 3 | Frost Nova (122, 2 percent, 25 s: roots enemies within 12 yd for up to 8 s, "damage caused may interrupt the effect") |
| 5 | Fire Blast (2136, instant, 8 s, 30 yd, 2 percent; replaced by Inferno Blast only for Fire) |
| 7 | Blink (1953, 20 yd, 15 s, 2 percent, frees from stuns and bonds) |
| 8 | Counterspell (2139, 24 s, 40 yd, 2 percent, 6 s school lock, not on the global cooldown) |
| 10 | Arcane Blast (30451, 2 s cast, 1.67 percent, generates a charge); Arcane Charge (114664 / aura 36032: up to 4 stacks, 10 s, each +50 percent damage and **+150 percent mana cost** on Blast); the specialisation is chosen here |
| 12 | Arcane Barrage (44425, instant, 3 s, 0.5 percent, consumes all charges, +50 percent damage per charge, hits 1 extra target per charge at 50 percent) |
| 14 | Polymorph (118, 1.7 s cast, 30 yd, up to 50 s) |
| 18 | Arcane Explosion (1449, instant, 3 percent, 10 yd, refreshes charges and may generate one) |
| 22 | Ice Lance (30455, instant, 1 percent; every mage has it) |
| 24 | Arcane Missiles (5143, 2 s channel of 5 waves, generates a charge, +50 percent per charge) with its proc "Arcane Missiles!" (79683, up to 2 charges for 20 s) |
| 26 | Ice Block (45438, 10 s, 5 min, Hypothermia afterwards) |
| 28 | Cone of Cold (120, instant, 4 percent, 10 s, slows 60 percent for 6 s) |
| 29 | Remove Curse (475, 8 s) |
| 32 | Slow Fall (130) |
| 34 | Molten Armor (30482, 3 s cast, critical strike up and physical damage taken down) |
| 36 | Slow (31589, 35 yd, 15 s: movement -50 percent, cast time +25 percent on players, one target at a time) |
| 38 | Conjure Refreshment (42955, 3 s cast; there is no Conjure Water or Food for a mage in the skill line) |
| 40 | Evocation (12051, 120 s: 15 percent of mana at once and another 45 percent over the 6 s channel, spell text) |
| 44 | Flamestrike (2120, 2 s cast, 6 percent, 8 yd) |
| 47 | Conjure Mana Gem (759, 3 s cast, 18 percent; makes item 36799) |
| 49 | Mirror Image (55342, 3 min, 30 s) |
| 50 | Wizardry (89744, +5 percent Intellect) |
| 52 | Blizzard (10, 5 percent, 8 s channel, 8 yd, slows 50 percent) |
| 54 | Frost Armor (7302, spell haste up, attackers slowed) |
| 56 | Invisibility (66, 5 min) |
| 58 | Arcane Brilliance (1459, party-wide spell power and critical strike) |
| 60 | tier 4 talent |
| 62 | Arcane Power (12042, 90 s, 15 s: +20 percent damage, +10 percent mana cost) |
| 64 | Spellsteal (30449) |
| 70 | Improved Counterspell (12598: adds a silence) |
| 74 | Nether Attunement (117957: haste also raises mana regeneration) |
| 75 | tier 5 talent (bombs) |
| 80 | Mastery: Mana Adept (76547); Mage Armor (6117: Mastery up, magic debuff duration down) |
| 84 | Time Warp (80353, 5 min) |
| 87 | Alter Time (108978, 3 min) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and data tables):

- Arcane Charge raises the damage of Blast, Missiles and Barrage by 50 percent per stack and the cost of Blast by 150
  percent per stack. A Blast at 0, 1, 2, 3 charges costs 1, 2.5, 4, 5.5 times the base (a fifth would cost 7 times), so
  damage per mana falls with every charge: stacking is a burst tool and a long fight needs a way to refill mana.
- Barrage ends the stack and its cost is a third of a Blast; it is also the cast-while-moving spell of the class.
- Evocation is learned at 40, the Mana Gem at 47: below 40 an Arcane mage has no emergency mana (derived).
- Arcane Missiles! comes from damage spells and holds 2 charges for 20 s, so a proc is rarely wasted if it is spent at
  4 charges (Method: wait for 4 even with 2 procs).

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` at commit `5927134`.

How the bots rank what they do (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, ACTION_NORMAL 10, ACTION_HIGH 20,
ACTION_MOVE 30, ACTION_INTERRUPT 40, ACTION_EMERGENCY 90. A trigger edge adds its action with that number; the engine takes
the highest action that is possible and useful; an action whose spell is unknown is simply skipped. An Arcane mage gets the
`arcane` and `arcane aoe` combat strategies plus `dps`, `dps assist`, `cure`, `cc` and `aoe` (`Factory/AiFactory.cpp:353-366`),
and every bot outside battlegrounds gets `boost`; non-combat it gets `bdps` (the armor strategy) and `cure`
(`AiFactory.cpp:722-729`). Cooldowns in `boost` use `BoostTrigger` (`GenericTriggers.cpp:416-424`): active at once against a
player, otherwise only when the group is out-matched 2 to 1 (balance 50 or less; inference from the rogue sheet, to check
live), so a normal 5-player pull never gets Mirror Image (the Arcane Power edge is a plain buff check and is not gated).

### Levels 15 to 29

Known: Frostfire Bolt, Frost Nova, Fire Blast, Blink, Counterspell, Arcane Blast, Arcane Barrage (12), Polymorph (14),
Arcane Explosion (18), Ice Lance (22), Arcane Missiles (24), Ice Block (26), Cone of Cold (28), Remove Curse (29).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | always | Arcane Blast (30451, 10) to build charges | yes, the default action at 5.6 (`ArcaneMageStrategy.cpp:68`) |
| 2 | core | 4 Arcane Charges | Arcane Barrage (44425, 12) to spend them | yes, 14.5 on `arcane blast stack` (`:88`; trigger `MageTriggers.h:148-152`) |
| 3 | core | moving | Arcane Barrage, then Fire Blast (2136, 5): the instant spells | yes, 5.4 and 5.3 (`:70-71`) |
| 4 | core | "Arcane Missiles!" up and 4 charges, from 24 | Arcane Missiles (5143, 24) | yes, 15.0 (`:86`); with fewer charges Blast (5.6) outranks Missiles (5.5), as Method wants |
| 5 | good | an enemy within 5 yd | Frost Nova (122, 3), then step away | yes, 50 (`GenericMageStrategy.cpp:210-211`); see the duel section for the late trigger distance |
| 6 | good | the enemy is frozen and close | Blink (1953, 7) away, then resume | yes, `blink back` 35 (`:212-213`, `MageActions.cpp:38-46`) |
| 7 | good | a target casts, from 8 | Counterspell (2139, 8) | **missing:** only an enemy healer (`:187`) and, in a battleground, arena or started duel, any enemy caster (`:221-222`); the plain `counterspell` trigger is registered (`MageAiObjectContext.cpp:108`) but no strategy schedules it |
| 8 | good | a second attacker or a marked target, from 14 | Polymorph (118, 14) | yes, `cc` strategy 30 (`GenericMageStrategy.cpp:284`; BT-028, deployed, live check pending) |
| 9 | good | 3 or more enemies within 8 yd of the target, from 18 | Arcane Explosion (1449, 18), 10 yd, refreshes charges | **missing:** the AoE edges start at Flamestrike (44) and Blizzard (52) (`GenericMageStrategy.cpp:294-301`, `ArcaneMageStrategy.cpp:94-98`); the action exists (`MageActions.h:66-70`) |
| 10 | good | an enemy in melee that has been rooted or has to be slowed, from 28 | Cone of Cold (120, 28) | **missing for Arcane:** only the Frost AoE strategy casts it (`FrostMageStrategy.cpp:168`) |
| 11 | expert | health at or under 25 percent | Ice Block (45438, 26) | yes, 90 (`GenericMageStrategy.cpp:193`); in a started duel only in a group and at 10 percent (`MageTriggers.cpp:192-199`), see the duel section |
| 12 | good | a party member is cursed | Remove Curse (475, 29) | yes, `cure` strategy 41 and 40 (`GenericMageStrategy.cpp:243-247`) |
| 13 | core | out of combat, no armor | none known until Molten Armor at 34 (derived: nothing to cast before) | n/a |

Before a spell is learned (derived): below 10 there is no specialisation and the kit is Frostfire Bolt, Frost Nova, Fire
Blast, Blink and Counterspell; the bot gets the `frost` strategy then (`AiFactory.cpp:280-287`), which is harmless. From 10
to 11 an Arcane mage has no Barrage and its Blast stack grows to 4 without a dump (to check, below the band). Missiles waits
for 24.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 14 | core | out of combat, armor missing | Molten Armor (30482, 34) until Frost Armor at 54 (derived: the only armor known) | **missing for Arcane:** `MageArmorTrigger` (Mage Armor, learned at 80) is the only armor edge for an Arcane mage (`MageTriggers.cpp:34-40`), `FrostArmorTrigger` excludes it (`:42-47`), `MoltenArmorTrigger` is Fire only (`:49-52`); so it casts nothing until 80 (read) |
| 15 | good | enemy melee player, a started duel or any time | Slow (31589, 36) | **missing:** the old edge was removed (`ArcaneMageStrategy.cpp:78-85` comment); PvE has no use, the duel section has one |
| 16 | core | low mana | Evocation (12051, 40), then a mana potion | yes, 90 (`GenericMageStrategy.cpp:196`, node alternative `:102`); below 40 the edge only fails |
| 17 | good | no conjured food in the bags, from 38 | Conjure Refreshment (42955, 38) | yes (`GenericMageNonCombatStrategy.cpp:62`, BT-029) |
| 18 | good | 3 or more enemies within 8 yd | Flamestrike (2120, 44), then Blizzard | yes, 23 and 22 (`GenericMageStrategy.cpp:294-301`): Flamestrike is a Fire spell in the guides; an Arcane mage has it from the class list (read) |
| 19 | good | a priority dispel target, from 64 | Spellsteal | yes, 40 (`:194`) |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | good | mana at the "high mana" threshold, gem in the bags, from 47 | Conjure Mana Gem (759, 47), then use the gem | yes, but only if the spell is known when the strategy is built (`GenericMageStrategy.cpp:239-240`); a mage that learns it after login keeps no edge until its strategies are rebuilt (inference, to check) |
| 21 | good | on a pack of elites or a boss, from 49 | Mirror Image (55342, 49) on cooldown (both guides) | partly: `boost` strategy, 28, gated by the balance rule above (`GenericMageStrategy.cpp:258`) |
| 22 | good | 3 or more enemies, from 52 | Blizzard (10, 52): channel, cancelled when fewer than 2 enemies are left | yes (`GenericMageStrategy.cpp:289`, `MageActions.cpp:49`) |
| 23 | core | out of combat, armor missing | Frost Armor (7302, 54): spell haste (Method and wowtbc: "maintain Frost Armor") | **missing:** the same armor edges as line 14; an Arcane mage never casts Frost Armor (read) |
| 24 | good | health low and enemies attack, from 56 | Invisibility (66, 56) as a threat dump (derived) | yes: two attackers, outside battlegrounds and duels, 30 (`GenericMageStrategy.cpp:195`, `MageTriggers.cpp:202-209`); a 5 minute cooldown |
| 25 | core | out of combat, party buff missing, from 58 | Arcane Brilliance (1459, 58) | yes, 21 (`GenericMageNonCombatStrategy.cpp:55`) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 26 | core | cooldown ready, fighting a pack or boss | Arcane Power (12042, 62) on cooldown (both guides), with Mirror Image | yes, 29 (`GenericMageStrategy.cpp:256`): a plain buff trigger, so it is cast on every ready cooldown, trash included |
| 27 | good | tier 4 talent | Cauterize passive, or Greater Invisibility | see Talents |
| 28 | good | casters in range | Counterspell with silence (Improved Counterspell, 70) | missing, see line 7 |
| 29 | good | not at 4 charges, moving | Barrage / Ice Floes (if picked) | Barrage yes; Ice Floes no |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | Living Bomb on one target, Nether Tempest on 2 to 4, Frost Bomb on 5 or more (both guides) | Nether Tempest as a debuff edge 19 (`ArcaneMageStrategy.cpp:90`) and on attackers in the AoE strategy 24 (`:96`); no Living Bomb or Frost Bomb for Arcane |
| 80 | Mastery: Mana Adept; Mage Armor is not in either guide (Frost Armor is) | Mage Armor is the Arcane armor edge (`MageTriggers.cpp:34-40`): differs from both guides |
| 84 | Time Warp on a big pull | missing (no action) |
| 87 | Alter Time at 4 charges with Arcane Power (both guides) | missing (no action) |
| 90 | single target (Method): Frost Armor, Rune of Power, Ice Floes when moving, Living Bomb, Mirror Image, Arcane Power with Alter Time, Missiles at 4 charges, Barrage at 4 charges, Blast; area: same without Living Bomb, over 5 targets Cone of Cold, Barrage at 4, Arcane Explosion spam | Rune of Power 50 (`GenericMageStrategy.cpp:198`), Arcane Power, Mirror Image, Missiles at 4, Barrage at 4, Blast, Nether Tempest are there; Frost Armor, Ice Floes, Alter Time, Cone of Cold, Arcane Explosion are not |

## Cooldowns and defensives

The guides say "on cooldown"; the timing for a dungeon bot is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Arcane Power | 62 (90 s) | on cooldown, with Alter Time and Mirror Image | at a pack of elites or a boss | on every ready cooldown (`GenericMageStrategy.cpp:256`) |
| Mirror Image | 49 (3 min) | on cooldown | at a pack of elites or a boss | `boost`, balance rule (`:258`) |
| Evocation | 40 (120 s) | not covered (Rune of Power replaces it at 90) | low mana (below 15 percent), when the tank holds the enemies | yes, 90 (`:196`) |
| Mana Gem | 47 | not covered | at the "high mana" threshold | yes (`:239-240`) |
| Frost Nova | 3 (25 s) | not covered | enemy within the radius of 12 yd and not yet rooted | at 5 yd, any current target (`:210-211`) |
| Blink | 7 (15 s) | not covered | to leave a rooted melee enemy | after Nova (`:212-213`) |
| Ice Block | 26 (5 min) | first in wowtbc's defensive order | health under 25 to 35 percent with nothing else ready | 25 percent (`:193`); solo duel never |
| Temporal Shield | talent 30 (25 s) | third in wowtbc's order; Method's pick | when attacked | yes, 28.5 (`:219`) |
| Greater Invisibility / Cauterize | talent 60 | second in wowtbc / Method's choice | threat dump / death | see Talents |
| Counterspell | 8 (24 s) | not covered | any enemy cast of a dangerous spell | healers and BG/duel casters only |
| Cold Snap | talent 60 | resets Ice Block, Frost Nova, Cone of Cold only (Method) | n/a for the premade | WotLK logic: `ColdSnapTrigger` waits for Icy Veins and Deep Freeze on cooldown (`MageTriggers.h:256-260`), which Cold Snap does not reset in 5.4.8 |

## Healer triage and mana (healers only)

Not applicable to healing. Mana is the pressure on this specialisation (derived): the Arcane Charge cost rule above makes a
long fight drain faster than for Fire and Frost, and below level 40 there is no Evocation. Mana-related lines the bots have:
Evocation at 15 percent (`GenericMageStrategy.cpp:196`), the Mana Gem, the dungeon drink rule (`GenericTriggers.cpp:50-60`,
below 50 percent between pulls, config `AiPlayerbot.DungeonDrinkMana`) and the generic drink at `LowMana` 15, `MediumMana` 40
(`conf/playerbots.conf.dist:674-675`). Proposed (derived, option for the owner): below level 40 spend the stack at 3 charges
instead of 4, because the 4th Blast costs 37 percent more mana than the 3rd (5.5 against 4 times the base) for 25 percent
more damage (2.5 against 2.0 times; arithmetic from the Arcane Charge values).

## Low level duels and kiting (owner report 2026-11-11)

The guides are level 90 PvE guides and say nothing about Frostbolt's chill, Frost Nova, Blink or Ice Block at low levels;
Method's Arcane page has one related line (Cold Snap resets Ice Block, Frost Nova and Cone of Cold only) and wowtbc puts Ice
Block first in its defensive order. Everything below is derived from the 5.4.8 spell data. For Arcane the tools are:

- Slow: Frostfire Bolt (1, all mages, movement -40 percent for 8 s, but an Arcane bot does not cast it: its list ends in
  `frostbolt`, a spell Arcane cannot cast, see gap 5), Cone of Cold (28, -60 percent for 6 s), Slow (36, -50 percent for 15
  s, +25 percent cast time on players). Arcane Barrage and Blast slow nothing.
- Control: Frost Nova (3, root 8 s), Counterspell (8, 24 s), Polymorph (14), Blink (7, 20 yd, 15 s), Ice Block (26), later
  Greater Invisibility or Cauterize (60), Spellsteal (64).
- Sequence for a duel against a melee opponent (derived): open at range; if the enemy is not slowed, slow it (Frostfire Bolt,
  then Cone of Cold at 28 and Slow at 36); when it reaches about 10 yd (Nova radius is 12 yd) Nova it, then walk or Blink
  out of reach before dealing damage that may break the root; resume at range; keep Counterspell for its heal or cast; Ice
  Block only when Nova and Blink are on cooldown.

What the bots do (read): in a started duel `in pvp instance` is true (`PvpRotationValues.cpp:189-195`), so the lane
`mage pvp melee enemy player` is active: Frost Nova 50, Blink back 35, Blazing Speed 34 when the enemy player is in melee
range (`GenericMageStrategy.cpp:225-228`), Counterspell on an enemy caster (`:221-222`), and the flee edge is off
(`MageTriggers.cpp:211-218`). The kite (`PvpKiteTrigger`, `RangeTriggers.cpp:63-119`, BT-145) runs only from a slowed
enemy (speed below 85 percent of the bot's) or steps out of reach of a rooted one. Where they fall short:

1. An Arcane or Fire bot never slows its enemy (no Slow edge, no Cone of Cold, Frostfire Bolt is not in the list), so BT-145
   keeps it standing: it trades blows at range 5 after Nova and Blink. Cone of Cold (28) and Slow (36) are the missing
   tools.
2. The Nova trigger distance is the generic `TooCloseDistance` 5 yd (`conf/playerbots.conf.dist:646`) although Nova's radius
   is 12 yd and `CastFrostNovaAction::isUseful` allows 10 (`MageActions.cpp:24-28`): the enemy is already hitting the bot
   when the root lands.
3. A solo duelist never casts Ice Block: the duel counts as a pvp instance and the rule there is "in a group and health at or
   under 10 percent" (`MageTriggers.cpp:192-199`); everywhere else it is 25 percent.
4. Counterspell on the duel opponent exists (BG/duel lane) but not in plain world PvP or PvE.
5. No Polymorph in 1 against 1 (a sheeped enemy cannot attack for up to 8 s on a player: distance to regain).

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Time Warp (84), Alter Time (87), the tier 5 bombs (75) and the tier 6 talents (90) are not
  available in the 15 to 60 band; Mirror Image is learned at 49, Arcane Power at 62, Frost Armor at 54, Arcane Brilliance at
  58. The lists above start without them.
- Both guides open with "Frost Armor"; it is learned at 54 here, Molten Armor at 34, and Mage Armor (80) is in neither
  guide. The bots' Arcane armor edge is Mage Armor (BT-027 gave it to an unspecialised and a group Frost mage).
- Method's "wait for 4 charges even with 2 Missiles procs" holds only from 24, where Missiles arrives; before that the
  stack is spent by Barrage alone.
- Rune of Power replaces Evocation (spell text), so Method's Evocation glyph only has a use with Rune of Power or
  Invocation; the glyph's text names both.
- Ice Floes, Presence of Mind and Blazing Speed share a tier: the bots have an action for the third only.
- The Arcane bot's default list ends in `frostbolt` "for arcane immune targets" (`ArcaneMageStrategy.cpp:72`); Frostbolt is
  a Frost specialisation spell (SpecializationSpells spec 64), so an Arcane mage cannot cast it. The shared Frostfire Bolt
  (level 1) is the fallback that exists.
- Spell Power is not an item stat in 5.x (inference); the bots' weight list still carries Spirit above Intellect.

## Gap list (ordered)

Config (can go at once):

1. Glyph order: `PremadeSpecGlyph.8.0` (`conf/playerbots.conf.dist:2846`) to `42748,42743,44955,45739,42735,42738`. Today
   a level 25 to 61 Arcane bot carries Arcane Power (nothing before 62) and Mirror Image (nothing before 49) in its first
   slots and the first useful glyphs only from level 50.
2. Tier 60: `Cauterize` (digit 1) instead of Greater Invisibility (digit 0): `PremadeSpecLink.8.0` (`:2844`) to `200101`.
   Reason: a passive cheat death needs no code, and the Invisibility cast is unverified. Owner question.
3. Tiers 15 and 45 are dead talents (Ice Floes, Ring of Frost): keep the guide picks only together with rotation items 9
   and 10, otherwise they cost nothing but also give nothing. Blazing Speed (digit 1 at tier 15) is the one with code.

Rotation (class fixes, wait for the owner's in-game test):

1. Armor (core, all levels 34 to 79): Arcane wears nothing between 34 and 79, and Mage Armor from 80 against both guides'
   Frost Armor. Fix `MageTriggers.cpp:34-52` (`MageArmorTrigger`, `FrostArmorTrigger`, `MoltenArmorTrigger`) and the nodes in
   `GenericMageNonCombatStrategy.cpp:67-79`: Arcane and Frost use Frost Armor when known, else Molten Armor when known;
   Fire uses Molten Armor; Mage Armor only when neither is known. To check live: BT-027 is deployed and its live check is
   pending.
2. Counterspell on the current target's cast (good): add `TriggerNode("counterspell", ...)` at 40 to
   `GenericMageStrategy::InitTriggers` (`:187`), using the registered `CounterspellInterruptSpellTrigger`
   (`MageTriggers.h:62-66`, `MageAiObjectContext.cpp:108`). Applies to all three specialisations.
3. Arcane Explosion as the AoE filler for 3 or more enemies within 10 yd from level 18 until Flamestrike (44) and Blizzard
   (52) take over, and Cone of Cold (28) as the second slow and the 5 plus target spell (both guides):
   `ArcaneMageAoeStrategy::InitTriggers` (`ArcaneMageStrategy.cpp:94-98`) and `MageAoeStrategy` (`GenericMageStrategy.cpp:294-301`).
   Guard against Polymorphed marked targets in the area (to check).
4. Slow for a melee player in a duel or PvP (36): an edge `enemy player in melee range` or `slow not on target` in
   `ArcaneMageStrategy::InitTriggers`; the action and trigger exist (`MageActions.h:41-46`, `MageTriggers.h:173-177`). With
   Cone of Cold and Frostfire Bolt in the list the BT-145 kite works for Arcane.
5. Arcane default list: replace `frostbolt` (`ArcaneMageStrategy.cpp:72`) with `frostfire bolt`, which every mage knows.
6. Mirror Image on a pack of elites or a boss (good): `MageBoostStrategy` (`GenericMageStrategy.cpp:258`) uses the balance
   gate; a rule for 3 or more attackers or a boss in addition to "outmatched 2 to 1". Arcane Power (`:256`) is ungated and
   fires on trash: gate it the same way for the 15 s window (derived). Good tier.
7. Duel Ice Block: `MageIceBlockHealthTrigger` (`MageTriggers.cpp:192-199`): a solo duelist never casts it, and a group
   duelist only at 10 percent. A rule for a started duel: health at or under 30 percent with Nova and Blink on cooldown.
8. Nova trigger distance for an enemy player: 8 to 10 yd instead of 5 (PvP only), `GenericMageStrategy.cpp:210-211` or a new
   `mage pvp` trigger beside `:225-228`.
9. Ice Floes action (self buff, before a cast that has to be made while moving) and the `ring of frost` action
   (ground-targeted at the pack) if the talents stay picked; both are expert-level extras. Presence of Mind and Alter Time
   are outside this band.
10. Mana: below level 40 dump the stack at 3 charges (option, see the mana section); owner decision.
11. Stat weights (gear scoring): Intellect above Spirit and Spirit to 0 for the mage branch (`StatsWeightCalculator.cpp:527-539`,
    `:712-720`), Mastery last below level 80. Applies to all three mage sheets.
12. `ColdSnapTrigger` (`MageTriggers.h:256-260`) waits for Icy Veins and Deep Freeze on cooldown, a WotLK rule; Cold Snap
    now resets Ice Block, Frost Nova and Cone of Cold. Only relevant if Cold Snap is ever picked.
13. Skill tiers: the mage strategies have no tier gating (`BotSkill` is used only in the tank code). Lines marked good and
    expert above (Counterspell, Polymorph, Blink-back, Mirror Image timing, Ice Block) are the ones a casual bot should drop.
14. To check live: BT-027 (armor), BT-028 (Polymorph on a second attacker) and BT-029 (Conjure Refreshment) are deployed with
    their live check pending; whether the `invisibility` action resolves with Greater Invisibility; whether Frost Nova in a
    dungeon roots the tank's enemies and breaks the mage's own Polymorph (the Nova deals damage; read, not tested).

Owner questions: (1) Tier 15 for a bot with no Ice Floes code: keep the guide pick, or Blazing Speed which works in
duels and battlegrounds today. (2) Tier 60: Cauterize (no code) or Greater Invisibility (guide-listed, code unverified).
(3) May the first glyph slots at 25 and 50 hold a glyph the guide ranks third (Rapid Displacement) so that every slot works at
its level. (4) Is the below-40 mana rule (stack at 3) wanted, or should Arcane burn through the guide's rotation.

## PvP playbook (arena, 5.4.8)

New section, 2026-11-19. Sources: player guides (5.x) and community guides for the spec (one short PvP guide of patch 5.4, three raid guides used only for the stack and mana loop), the 5.x patch notes, the server's own data (`data/dbc/` through `spec_data.py` and short read-only scripts) and the module code read on `main`. Lines say "(guide)" when a player guide gives them, "(adapted)" when they come from the Frost mage arena material and were checked against the Arcane spell list, "(derived)" when they follow from the 5.4.8 data and game knowledge. A guide is rarely found for this spec: about a quarter of the section is sourced, a quarter adapted and half derived. Where nothing reliable exists the line says GAP.

Where the spec stands: no Arcane mage is in the MoP Classic EU top 100 ladder of 2026-11-18 (96 players in 3v3, 54 in 2v2), and a community tier list of July 2025 rates it C in 3v3 and C in 2v2. It is a niche pick: the value of a rule is lower than for a ladder spec.

### Main plan (derived from the guide)

1. Build charges only behind control (Slow, Frost Nova, Cone of Cold, Blink, a partner's crowd control) and spend them in a window; charges last 10 s (data), so they are not kept as a standing buff.
2. The window is Presence of Mind, Arcane Power and Alter Time with four charges: build to four, Arcane Power, Alter Time (it records location, health, mana, buffs and debuffs), Arcane Barrage, press Alter Time again before 6 s are over (the charges and the mana come back), Barrage again (guide; the order of the first presses is the guide's, the 6 s limit and the restored mana are in the spell text).
3. Take the healer out with Counterspell and a freeze (Frost Nova or Frostjaw, then Deep Freeze) or a Polymorph, and use the 40 yd instants (Barrage, Fire Blast) while kiting.

### Opener and positioning

- GAP: no source covers the prep room, the first position or the first target.
- Prep room (derived): Arcane Brilliance (58), a Mana Gem from 47 (Glyph of Mana Gem holds 10 charges), armor: Molten Armor (34) against melee, Mage Armor (80; its data text shortens harmful magic by 25 percent) against casters (adapted).
- Start behind the partner's side of a pillar, Living Bomb on the kill target, Counterspell the first heal or crowd control cast, Slow the melee that chases (derived). Stand at 40 yd: Blast, Missiles and Barrage all reach it (data).
- Ice Floes (tier 15) lets the next three spells under 4 s cast while moving, 20 s to recharge since 5.4.0: it fits a bot that has to kite (derived); Presence of Mind fits crowd control (guide).

### Priority outside a window (derived from the guide)

| Priority | Tier | Line |
|---|---|---|
| 1 | core | Counterspell on a crowd control cast, then on a big heal on the kill target; Improved Counterspell (70) adds a 4 s silence |
| 2 | core | While kiting: Arcane Barrage (instant, 3 s cooldown, 40 yd), Fire Blast (8 s) and Ice Lance (22) as the casts that can move; Barrage and Fire Blast also proc Arcane Missiles (guide) |
| 3 | good | Living Bomb on the kill target (12 s, 1 s global cooldown); refresh it before 4 s are left when a polymorph is up, because the blast hits up to three enemies within 10 yd (adapted) |
| 4 | good | Arcane Missiles on the proc at three or four charges (it refreshes the stack and regenerates mana, raid guide) |
| 5 | core | Arcane Blast to build or refill only when no melee is on you; a Blast at four charges costs seven times the base mana |
| 6 | good | When Arcane is school locked: Frostfire Bolt, Fire Blast, Living Bomb, Ice Lance |
| 7 | expert | Barrage only when no crowd controlled enemy stands near the target: it hits one more nearby enemy per charge for half damage and wakes a sheep (derived from the spell text) |

Stat order (guides disagree): the PvP guide says haste first (more casts per global), the raid guides say mastery first because the bonus grows with unspent mana. Arena damage keeps mana low, so for arena rolls take haste, then mastery (derived).

### Crowd control (derived and adapted)

- Arcane freezes only with Frost Nova (25 s) and Frostjaw (20 s, silence and root, half length against players): Frozen Orb and the pet freeze are Frost spells and Dragon's Breath is Fire (specialisation lists).
- Healer lock: Frostjaw or Frost Nova, then Deep Freeze (66, 30 s, 35 yd, 5 s, class wide), with the Glyph of Deep Freeze (off the global cooldown); or Counterspell then Polymorph (14). One Deep Freeze per 30 s is the cadence (adapted: the Frost course ties it to Fingers charges, which Arcane lacks).
- Polymorph from Presence of Mind is instant, so the healer cannot kick it (guide, adapted). Ring of Frost shares the incapacitate category with Polymorph.
- Slow (36): one target, 15 s, 50 percent movement, 25 percent more cast time on players; the Glyph of Slow makes Arcane Blast apply it (data).

### Defensives

Temporal Shield (30; 4 s, 15 percent less damage, healed back, usable stunned; since 5.4.2 its heal is cut by healing reduction) into burst, Flameglow into steady damage, Ice Barrier is stealable (guide). Blink (7) breaks stuns unless the Rapid Displacement glyph is worn. Cold Snap (60) resets Ice Block, Frost Nova and Cone of Cold and heals 30 percent; Greater Invisibility (60, 90 s) is 90 percent less damage for 20 s; Cauterize survives one killing blow. Alter Time is also an emergency heal because it returns health. Ice Block (26) is last (5 min, 30 s of Hypothermia). All derived from the spell text except the Ice Barrier and Temporal Shield remarks (guide).

### Matchups and partners

GAP: no sourced matchup. Derived: melee cleave: Slow, Nova, Cone, Blink, Molten Armor with the Armors glyph; casters: Mage Armor, Counterspell, Spellsteal; hunters: Ring of Frost against a Stampede (adapted); partners: a stun or fear partner that locks the kill target while Arcane locks the healer.

### PvP set (no ladder data)

Premade today: `PremadeSpecLinkPvp.8.0 = 000212` (Presence of Mind, Temporal Shield, Ring of Frost, Cold Snap, Living Bomb, Incanter's Ward) and glyphs `69773,92727,42752,42743,42735,42744` (Armors, Discreet Magic, Polymorph, Momentum, Loose Mana, Ice Block); it is the Frost set. The guide agrees on tiers 15, 30, 60 and 75 and names Frostjaw at tier 45. No ladder player holds the spec, so nothing is changed on ladder grounds.

### Differences from the guides (5.4.8)

- Frozen Orb and Dragon's Breath are not Arcane spells; Deep Freeze is class wide.
- Mage Armor shortens harmful magic by 25 percent (one guide says 35); Incanter's Ward gives 15 percent (30 before 5.3); Temporal Shield's heal obeys Battle Fatigue (5.4.2); Living Bomb and Nether Tempest deal 85 percent to players (5.4.0).
- Learn levels: Alter Time 87, Time Warp 84, Mirror Image 49, Arcane Power 62, Counterspell 8, Improved Counterspell 70, Spellsteal 64, Deep Freeze 66; below those levels the line does not apply.

### Bots today (read on `main`, 2026-11-19)

| Rule | Bots today |
|---|---|
| Barrage dump at four charges, Missiles on the proc | exists (`ArcaneMageStrategy.cpp:88-90`) |
| Slow and Cone of Cold on a melee player, Frost Nova, Blink away, Blazing Speed | exists (`ArcaneMageStrategy.cpp:80,92`; `GenericMageStrategy.cpp:189,213-216,230-234`) |
| Counterspell on a crowd control cast or a healer heal | exists (`GenericMageStrategy.cpp:188-190,226-228`, through the interrupt gate) |
| Arcane Power and Mirror Image in a go | missing for players: they fire only on a boss or an elite pack (`MageTriggers.cpp:225-231`) |
| Presence of Mind routed to Frostjaw or Polymorph; Alter Time with four charges | missing for Arcane (Fire only: `MageTriggers.cpp`, `FirePresenceOfMind`, `FireAlterTime`) |
| Living Bomb on the kill target | missing for Arcane: the code casts Nether Tempest (`ArcaneMageStrategy.cpp:94`); the PvP set picks Living Bomb, a dead talent today |
| Frostjaw then Deep Freeze, Polymorph the healer | missing (Deep Freeze only in the Frost strategy, `GenericMageStrategy.cpp:181-186`; Polymorph only on the dungeon mark, `:288-290`) |
| Alter Time as a defensive | missing |
| Temporal Shield before expected damage | partly (`GenericMageStrategy.cpp:222`, on "being attacked") |
| Ring of Frost under a melee player | partly (talent policy, `Utils/TalentSpellPolicy.h`) |
| Ice Floes | missing (no action) |

### Gap list (ordered by value; rotation items wait for the owner's in-game test)

Config:

1. `PremadeSpecLinkPvp.8.0` tier 75: keep Living Bomb only when a Living Bomb edge exists for Arcane; until then Nether Tempest (digit 0) is the pick the code casts (derived).
2. Add the Glyph of Counterspell (it lets Counterspell be cast while casting, +4 s cooldown) and the Glyph of Slow to the PvP glyphs in place of Discreet Magic and Loose Mana (derived; item ids to look up).
3. Tier 45: Frostjaw instead of Ring of Frost once a Frostjaw edge exists for Arcane (guide).

Rotation:

1. A go for Arcane against a player (expert): Presence of Mind, Mirror Image, Arcane Power at four charges, Alter Time, Barrage, second press, Barrage.
2. Living Bomb for Arcane with the sheep rule (good).
3. Frostjaw plus Deep Freeze, or Counterspell plus Polymorph, as the healer lock (expert).
4. Barrage only when no crowd controlled enemy is near the target (expert).
5. Alter Time as an emergency heal below 35 percent when the burst is over (expert).
6. Ice Floes while kiting with Blast or Missiles (good).
