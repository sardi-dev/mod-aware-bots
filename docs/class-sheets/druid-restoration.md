# Druid: Restoration (healer)

- **Status:** checked against 5.4.8 (2026-11-10). Implemented, compile and live check pending: gap list items 1 to 4
  (owner approved 2026-11-10, branch `feat/class-sheet-druid-warrior`); the "bots today" columns below describe the
  state before them. Every "bots today" claim below was checked by reading the call sites unless it says "to check".
- **Sources:** Method, "Restoration Druid Class Guide - MoP Classic" (Devorta, 22 July 2025, Phase 1, level 90 raids
  and dungeons), read 2026-11-10; wowtbc.gg, "Restoration Druid PvE Guide, MoP Classic" (the page the module's premade
  comments cite), read 2026-11-10; the server's own data files (Spell.dbc, SpellLevels.dbc, SpecializationSpells.dbc,
  Talent.dbc, GlyphProperties.dbc, Item-sparse.db2 of build 18414) for every spell id, learn level, talent tier and
  glyph. Overgear and Icy Veins answered HTTP 403 and Wowhead is not readable, so there are two guides. Neither has a
  levelling section, a dispel section or triage thresholds: the level bands, triage and mana lines are derived (marked
  "derived").
- **Levels covered:** 15 to 90, with the detail on 15 to 60 (dungeons with five players).

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.11.3 = 000012` (one digit per
tier, the column counted from 0; the config comment names the same six talents).

| Tier (level) | Pick | Alternative and when | Spell id | Bots today |
|--------------|------|----------------------|----------|------------|
| 1 (15) | Feline Swiftness (Method default) | Displacer Beast (blink); Wild Charge (wowtbc lists it) | 131768 | Feline Swiftness, same |
| 2 (30) | Ysera's Gift (Method: the one fixed talent) | Renewal, Cenarion Ward: the bot has no edge that casts either, so Ysera's Gift (passive) stays | 145108 | Ysera's Gift, same |
| 3 (45) | Faerie Swarm | Typhoon, Mass Entanglement by fight (Method: interchangeable) | 106707 | Faerie Swarm, same |
| 4 (60) | Soul of the Forest (Method default throughput) | Incarnation: Method for burst fights, wowtbc for a raid cooldown; Force of Nature | 114107 | Soul of the Forest, same |
| 5 (75) | Ursol's Vortex (neither guide names a default; all three are control talents) | Mighty Bash, Disorienting Roar | 102793 | Ursol's Vortex, same |
| 6 (90) | **Heart of the Wild** (Method default) | Nature's Vigil (Method names it as the other option); Dream of Cenarius (wowtbc only) | 108288 | **Nature's Vigil (124974), differs: no restoration edge ever casts it** |

Which guide was richer: Method gives the defaults and the reasons per tier; wowtbc lists the options only. They agree
on every tier except that wowtbc also lists Wild Charge, Renewal, Cenarion Ward and Dream of Cenarius.

## Glyphs

Majors unlock at 25, 50 and 75, minors in the same pairs (BotFactory.cpp:1338): a configured glyph goes into the first
free slot of its type, in config order, so the order of the majors decides what is worn at 25 to 49, 50 to 74 and 75 and up.

| Type | Glyph | Item id | Spell id (GlyphProperties) | Why | Bots today (PremadeSpecGlyph.11.3 = 45602,43332,40915,104102,43316,40912) |
|------|-------|--------:|---------------------------:|-----|------------|
| Major | Regrowth | 40912 | 116218 | wowtbc standard; works from level 18, the first one worth wearing | slot 3 (worn from 75) |
| Major | Rejuvenation | 40913 | 17076 | Method: mandatory; works from level 4 (effect text not read, to check on Wowhead) | not in the build |
| Major | Wild Growth | 45602 | 62970 | both guides; needs Wild Growth (level 76) | **first major, worn from 25, does nothing until 76** |
| Major | Efflorescence | 40915 | 145529 | both guides; needs Wild Mushroom (level 82 to 84) | **second major, worn from 50, does nothing until 84** |
| Major | Rebirth, Stampeding Roar | 40909 (Rebirth) | 54733, 114222 | Method optional | not in the build |
| Minor | Grace | 43332 | 114295 | wowtbc standard | yes |
| Minor | Sprouting Mushroom | 104102 | 146654 | wowtbc standard | yes |
| Minor | Aquatic Form | 43316 | 57856 | filler, weak (Method: all minors are weak) | yes |

The item names were read from the client's Item-sparse.db2; the world database's old 2014 item_template file names
40915 "Lifebloom" and 43332 "Thorns" and is stale, so the live `item_template` rows are to be checked once.

## Stat order

Method: Haste to its soft caps (level 90 numbers), then Spirit, then Mastery, then Critical Strike; wowtbc gives none.
For levels 15 to 89 (derived: the caps are level 90 values that no gear reaches earlier, and Spirit is the healer's
mana): **Intellect, then Spirit, then Haste, then Mastery, then Critical Strike.** Leather Specialization comes at 50
(data), so before 50 cloth with Intellect and Spirit competes with leather. For loot rolls and quest rewards:
Intellect and Spirit leather or cloth, never Agility leather; a piece with Spirit beats the same piece without.

## What the specialisation knows, by level (5.4.8 data)

| Level | Spell (id) |
|------:|------------|
| 1 | Wrath (5176) |
| 3 | Moonfire (8921) |
| 4 | Rejuvenation (774) |
| 10 | Swiftmend (18562), Natural Insight (112857, passive) |
| 12 | Nourish (50464), Revive (50769, out of combat resurrection) |
| 14 | Meditation (85101, passive mana regeneration in combat) |
| 16 | Travel Form (783), Nature's Focus (84736, passive) |
| 18 | Regrowth (8936), Aquatic Form (1066) |
| 22 | Nature's Cure (88423, dispel) |
| 26 | Healing Touch (5185) |
| 28 | Living Seed (48500, passive) |
| 30 | Nature's Swiftness (132158) |
| 36 | Lifebloom (33763) |
| 38 | Omen of Clarity (113043, passive, gives Clearcasting) |
| 44 | Barkskin (22812) |
| 46 | Swift Rejuvenation (33886, passive) |
| 50 | Leather Specialization (86093) |
| 54 | Innervate (29166) |
| 56 | Rebirth (20484, resurrection in combat) |
| 58 | Flight Form (33943) |
| 60 | tier 4 talent; Mark of the Wild (1126) is level 62 in SpellLevels, three other "Mark of the Wild" rows say 14 (to check what a level 15 to 61 druid really has) |
| 64 | Ironbark (102342) |
| 74 | Tranquility (740) |
| 76 | Wild Growth (48438) |
| 80 | Mastery: Harmony (77495) |
| 82 | Malfurion's Gift (92364), Wild Mushroom (138611) |
| 84 | Wild Mushroom: Bloom (102791) |
| 88 | Genesis (145518) |

Tree of Life is not a form here: it is the 30 second, 3 minute Incarnation talent (tier 4), aura 33891 (the module knows
this: `DruidShapeshiftActions.h:32-44`). A healer that took Soul of the Forest never has it.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only).
How the bot picks a heal today (read): `PartyMemberToHealValue.cpp:33` returns the group member with the lowest health
percent (distance adds health + distance / 10, beyond HealDistance 38.5 adds 30, members at or above 65% who are already
the target of a heal are skipped); the trigger chains in `HealDruidStrategy.cpp` then choose the spell by that member's
health: critical below 25 (conf), low below 45, medium below 65, almost full below 85. The tank is not preferred.

### Levels 15 to 29
| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | a member is dead, out of combat | Revive (50769, 12) | yes: `GenericDruidNonCombatStrategy.cpp:119` at 40, leaves any form first (`DruidActions.cpp:75`) |
| 2 | core | pull starts, the tank has no Rejuvenation (derived: a HoT costs the least mana and is what Swiftmend and Nourish want on the target) | Rejuvenation (774, 4) on the tank | **gap:** no tank-aimed edge; Rejuvenation comes only from the medium chain (`HealDruidStrategy.cpp:135`), after someone is below 65% |
| 3 | core | a member below 25% | Swiftmend (10, needs a HoT on the target), Regrowth (18), Nourish (12), Healing Touch (26), in that order | yes: `HealDruidStrategy.cpp:73-84` |
| 4 | core | below 45% | Regrowth (18), Swiftmend, Nourish, Healing Touch | yes: `HealDruidStrategy.cpp:115-124` |
| 5 | core | below 65% | Swiftmend, Rejuvenation, Regrowth, Nourish, Healing Touch | yes: `HealDruidStrategy.cpp:127-140` |
| 6 | good | below 85%, mana above 60% | Rejuvenation, Regrowth | yes: `HealDruidStrategy.cpp:148-152` |
| 7 | good | a poison or curse on a member, from 22 | Nature's Cure (88423) | partly: poison and curse yes (`GenericDruidStrategy.cpp:153-159`, `DruidActions.cpp:20`), **magic missing** (no Magic trigger for druids) |
| 8 | good | Nourish is the cheap filler on a member who already has a HoT (derived, effect to verify on Wowhead 50464); Regrowth is the expensive emergency | Nourish | partly: Nourish is in every chain but never chosen for "has a HoT" |
| 9 | core | mana below 50%, nobody of the group in combat | drink | yes: `UseFoodStrategy.cpp:49`, `GenericTriggers.cpp:51` (dungeon groups) |
| 10 | good | nobody needs healing, not in a dungeon | Moonfire (3), Wrath (1) | yes: `GenericDruidStrategy.cpp:242-251` ("healer dps"; off on dungeon maps, `AiFactory.cpp:578-581` and the config list) |
| 11 | expert | idle in a dungeon | nothing: no damage spells, HoT top-ups only | yes (healer dps is off there) |

### Levels 30 to 44
As above, plus:
| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 12 | good | a member below 25% | Nature's Swiftness (30) then Healing Touch (26), instant | partly: Healing Touch follows the proc (`HealDruidStrategy.cpp:97`, flat 55), but **Nature's Swiftness is cast on cooldown at every fight start** (`DruidBoostStrategy`, `GenericDruidStrategy.cpp:165`, 29), not for an emergency; the critical edge (`:86-88`) is redundant. Inference, to check live: the Healing Touch lands on whoever is lowest, even at 95% |
| 13 | core | from 36, in combat | Lifebloom (33763) on the tank, kept at 3 stacks, refreshed in the last seconds (both guides) | **gap:** the action exists (`DruidActions.cpp:126`) but its only edge is "clearcasting" (`HealDruidStrategy.cpp:92`); Lifebloom is never cast otherwise |
| 14 | good | Clearcasting (Omen of Clarity, 38) is up | spend it on Regrowth or the Lifebloom refresh (free) | partly: Lifebloom refresh only (`HealDruidStrategy.cpp:92`) |
| 15 | good | from 44, self below 45% | Barkskin (22812) | yes: `GenericDruidStrategy.cpp:114`; also a flat edge at 40 on "almost full health" (`:139`), to check whether it burns the cooldown on the first scratch |

### Levels 45 to 59
As above, plus:
| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 16 | good | mana below 40%, from 54 | Innervate (29166) on self; mana potion | yes: `HealDruidStrategy.cpp:154` (25), potion at 90 (`UsePotionsStrategy.cpp:10`) |
| 17 | core | a member is dead, in combat, from 56 | Rebirth (20484), in range | yes: `GenericDruidStrategy.cpp:133` (29) |
| 18 | core | save mana: below 60% mana skip top-ups | Rejuvenation and Wild Growth only on a clear loss; Nourish and Healing Touch only for members below 25% | yes (shared): `ConserveManaStrategy.cpp:14`, added at `AiFactory.cpp:577` |

### Level 60 and above
| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 19 | good | from 64, the tank below 40% | Ironbark (102342) | yes: `HealDruidStrategy.cpp:69`, trigger `DruidTriggers.cpp:318` |
| 20 | good | from 74, three members below 65% in a full group | Tranquility (740) | yes: `HealDruidStrategy.cpp:108-112` (needs five group members near, `HealthTriggers.cpp:31`) |
| 21 | core | from 76, three or more members below 85% | Wild Growth (48438), after Swiftmend when Soul of the Forest | yes for the cast (`HealDruidStrategy.cpp:100-106`); the Swiftmend pairing is not coded (expert) |
| 22 | expert | from 82 to 88 | Wild Mushroom, Wild Mushroom: Bloom, Genesis, a direct heal every 20 s for Harmony | **missing:** none of the three is in the module |
| 23 | expert | at 90, Soul of the Forest | Swiftmend, then Wild Growth; Tranquility with it | missing |

## Cooldowns and defensives

- **Nature's Swiftness** (30), **Swiftmend** (10), **Ironbark** (64), **Tranquility** (74), **Innervate** (54): see lines
  above. Guides order the raid cooldowns Tranquility, Nature's Vigil, Ironbark, Genesis, Innervate, Might of Ursoc,
  Barkskin (wowtbc); only Tranquility, Ironbark, Innervate and Barkskin exist in the bots for a healer.
- **Might of Ursoc:** an edge for every druid at health 25% or less (`GenericDruidStrategy.cpp:120`, level 72 in data;
  a level 52 row also exists). In 5.4.8 it puts the druid in bear form; the heal edges have no "leave the form" step,
  so a healer may lose seconds. To check live.
- **Nature's Vigil, Heart of the Wild, Incarnation, Dream of Cenarius:** no resto edge. Nature's Vigil has an edge for
  balance and guardian only (`GenericDruidStrategy.cpp:184,191`).

## Healer triage and mana

- **Who first (derived from the guides' logic and the thresholds):** 1 a member below 25%, instant or short casts
  (Nature's Swiftness with Healing Touch, Swiftmend, Regrowth); 2 the tank below 40%, Ironbark (64); 3 the tank's
  upkeep: Rejuvenation always, Lifebloom at three stacks from 36; 4 anyone else, lowest percent first; 5 group heal when
  three or more are hurt (74 and up); 6 top-ups with Rejuvenation. Today: lowest percent first for everyone, no tank upkeep.
- **Dispels:** Nature's Cure takes poison, curse and magic from a friend (module comment from Wowhead 88423). Dispel edges run at
  priority 51 and 57 (`ACTION_DISPEL` 50), above every heal (30 to 34.5), so a harmless poison can delay a heal at 10%.
  Derived rule: dispel only when no member is below 45%. To check: what `NeedCureTrigger` filters on.
- **Mana, in a fight:** below 60% the shared save-mana rule blocks Nourish and Healing Touch on members at 25% and above and
  blocks Swiftmend and Tranquility on members at 65% and above (`ConserveManaStrategy.cpp:31-54`). At 40% a mana potion
  (90) and, from 54, Innervate (25). Out of a fight: drink below 50% in a dungeon (`DungeonDrinkMana`), 15% elsewhere.
  Derived: the combat healer should keep Rejuvenation as the cheap spell and not cast Regrowth for top-ups.
- **Out of combat top-ups:** Wild Growth, Regrowth and Rejuvenation on hurt members (`GenericDruidNonCombatStrategy.cpp:128-153`).
  Derived: Rejuvenation only, then drink; Regrowth before drinking spends the mana the drink must give back. The Regrowth
  node is registered under the key "regrowth_on_party" (`:19`), so its caster-form step is never attached (to check).
- **Resurrection:** out of combat Revive (12, `:119`), in combat Rebirth (56, `GenericDruidStrategy.cpp:133`); both leave any form first.
- **Forms:** caster form for everything. The in-combat heal edges have no "leave the form" step (`HealDruidStrategy.cpp:73-140`):
  a resto druid that follows its leader in travel form (`CheckMountStateAction.cpp:734`) may start a fight in it. To check
  live whether 5.4.8 casts a heal out of a form. Aquatic Form under water, out of combat (`GenericDruidNonCombatStrategy.cpp:161`).
- **Nothing to heal:** in a dungeon nothing (healer dps is off on dungeon maps); in the open world Moonfire and Wrath when
  every member is at 85% or more and mana is above the threshold (`GenericTriggers.cpp:434`).

## Differences from the guide (5.4.8 against MoP Classic)

- Both guides are level 90. Efflorescence, Wild Mushroom, Genesis and Wild Growth are levels 82 to 88 or 76; below those
  the glyphs for them do nothing (see Glyphs).
- Healing Touch is level 26 here (data), not a level 1 spell; Regrowth is 18, Lifebloom 36, Swiftmend 10.
- The guides never mention Revive, Rebirth or dispels except Rebirth as a glyph; the lines for them are derived.
- Haste caps (8033, 10559, 10970, 13162) are level 90 numbers and are left out below 90.
- Method's default for the last tier is Heart of the Wild; the bots take Nature's Vigil, which nothing casts.
- wowtbc's glyph list has Regrowth, Method's Rejuvenation; the data has both (levels 18 and 4). Efflorescence and Wild
  Growth are in both guides and are kept for 76 and up.

## Gap list (ordered by value for levels 15 to 60 in dungeons)

1. Lifebloom on the tank from level 36 (core, both guides): the action exists, only a Clearcasting edge uses it.
   **Implemented:** trigger "lifebloom on main tank", priority 20.8 (`HealDruidStrategy.cpp`).
2. Rejuvenation on the tank at the pull (derived, core): a tank-aimed edge. **Implemented:** action and trigger
   "rejuvenation on main tank", priority 20.9; it keeps the HoT on the tank for the fight while mana is above the
   save-mana threshold (a pull-only window needs a combat clock the bots do not have).
3. Nature's Swiftness spent on cooldown at a fight start instead of an emergency (good; verify live).
   **Implemented:** removed from "boost" for the restoration specialisation; the critical health edge casts it, and the
   Healing Touch that follows waits for a member below the low health threshold ("nature's swiftness heal").
4. Magic dispels with Nature's Cure from level 22 (good), and dispels below emergency heals. **Implemented:** triggers
   and actions "nature's cure magic" and "nature's cure magic on party" in the cure strategy; silent while a member is
   below the critical health threshold. The existing poison and curse dispel edges are unchanged.
5. Premade last tier: Heart of the Wild instead of Nature's Vigil (config).
6. Premade major glyph order: Regrowth, Rejuvenation, Wild Growth (config); Efflorescence kept for the future.
7. Out-of-combat top-ups with Rejuvenation only (good).
8. Nourish on a member with a HoT, Swiftmend after a Rejuvenation on the same member (expert).
9. In-combat heals from a form, Might of Ursoc for a healer (to check live).
10. Level 76 and up: the Swiftmend then Wild Growth pairing, Wild Mushroom, Genesis, Harmony upkeep (expert, later).

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 8 players of this specialisation (rating 2455 to 2910). 2v2: 5 players (rating 2658 to 2900). Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** 3v3 (8 players)
- **BT-324 result:** updated (BT-450); 2v2 differs on the Cheetah (BT-454)
- **Set now:** `PremadeSpecLinkPvp.11.3 = 122000`, `PremadeSpecGlyphPvp.11.3 = 45622,43332,40915,104102,89868,67484` (was `45622,43332,40915,104102,43316,67484`)

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Displacer Beast 100% / 100% / 13 of 13 | Displacer Beast | Displacer Beast | matches |
| 2 (30) | Cenarion Ward 100% / 100% / 13 of 13 | Cenarion Ward | Cenarion Ward | matches |
| 3 (45) | Typhoon 100% / 100% / 13 of 13 | Typhoon | Typhoon | matches |
| 4 (60) | Soul of the Forest 100% / 100% / 13 of 13 | Soul of the Forest | Soul of the Forest | matches |
| 5 (75) | Disorienting Roar 75% / 100% / 11 of 13, Mighty Bash 25% / 0% / 2 of 13 | Disorienting Roar | Disorienting Roar | matches |
| 6 (90) | Heart of the Wild 100% / 100% / 13 of 13 | Heart of the Wild | Heart of the Wild | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Efflorescence (40915) | 88% | 100% | 12 of 13 | yes | yes |
| major | Cyclone (45622) | 62% | 100% | 10 of 13 | yes | yes |
| major | Fae Silence (67484) | 88% | 60% | 10 of 13 | yes | yes |
| major | Barkskin (45623) | 62% | 40% | 7 of 13 | no | no |
| minor | the Sprouting Mushroom (104102) | 88% | 100% | 12 of 13 | yes | yes |
| minor | the Cheetah (89868) | 62% | 20% | 6 of 13 | no | yes |
| minor | Grace (43332) | 12% | 20% | 2 of 13 | yes | yes |
| minor | Aquatic Form (43316) | 0% | 20% | 1 of 13 | yes | no |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **What changed (3v3, 8 players):** the minor slot that held Aquatic Form (0 percent) now holds the Cheetah (62 percent in 3v3; it changes only the travel form's look). All six tiers (tier 75 Disorienting Roar 75/100) and the majors Efflorescence 88/100, Fae Silence 88/60, Cyclone 62/100 already matched (Barkskin 62/40 is the fourth major, no slot).
- **2v2 disagrees (BT-454), 3v3 pick kept:** only 1 of 5 resto druids in 2v2 takes the Cheetah (pooled 6 of 13, 46 percent). The 3v3 pick stays because the change is cosmetic either way. Grace (12/20) stays in its minor slot; the Sprouting Mushroom 88/100 is held.

## PvP class package: bots today (BT-668 to BT-672, 2026-11-19)

Status of the digest's section 12 rules after the Restoration package (see `docs/pvp/class-packages.md`; all wait for the owner's in-game test):

| Digest rule | Bots today |
|---|---|
| 1 Mastery buff and 3 stack Lifebloom | partly, as before (Lifebloom on the main tank, Rejuvenation upkeep; no Mastery read) |
| 2 Swiftmend for Soul of the Forest | partly (Swiftmend in the chains; no spend of the proc) |
| 3 Cenarion Ward and Ironbark before a go | done (BT-668, BT-669) |
| 4 first Cyclone on an off damage dealer, swap onto the healer | the healer Cyclone is a link of the shared chain (BT-629); the off damage dealer Cyclone is not built |
| 5 Nature's Swiftness Cyclone on a damage dealer | done (BT-670) |
| 6 pre-swap Bear Form and pre-HoTs | not built; Barkskin early is the shared defensive layer |
| 7 Displacer Beast after the enemy gap closer | done as an escape after the melee connected (BT-671); the enemy's mobility is not in the awareness book, so no wait for the gap closer |
| 8 Might of Ursoc | as before (25 percent health) |
| 9 Heart of the Wild in the opener | as before (talent policy, an enemy player) |
| 10 Wild Mushroom and Bloom | not built |
| 11 Disorienting Roar or Mighty Bash before the healer Cyclone | not built as a chain link; Disorienting Roar is the self peel of the talent policy |
| 12 Cyclone beside a caster partner's cast | not built |
| 13 Tranquility out of sight | not built |

Set: unchanged. Displacer Beast, Cenarion Ward and Typhoon, all 100 percent of the ladder, now have rules; the Fae Silence glyph (88 percent) still has none.
