# Hunter: Marksmanship (damage, ranged)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). Trap placement with
  Trap Launcher is left for later. The "bots today" columns below describe the code before the 2026-11-11
  implementation.
- **Sources:**
  - Method, "Marksmanship Hunter Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 23 July 2025),
    read 2026-11-11 through a scripted fetch (the talent build is an image: only the talent hints in the text are usable).
  - wowtbc.gg, "Marksmanship Hunter" in the MoP class guides (MoP Classic, no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellMisc, SpellRange, SpellEffect; `data/db2/Item-sparse.db2` for glyph item ids; the class
    trainer rows of `data/world_db` for the levels spells are taught at) through `spec_data.py` and short read-only scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, not retried), Wowhead (needs a browser).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's
  marks), 61 to 90 briefly from the guides.

Which source was richer: **talent hints (named swaps), the glyph order (Aimed Shot first), stat order, the Focus-spending rule
(Aimed Shot on fresh targets, Arcane Shot otherwise), the Steady Focus rule** Method (wowtbc lists every talent option without
a pick and has no stat order); **the Master Marksman rule (Aimed Shot at 3 stacks), the AoE split by target count, the Fervor
label (Tier 14), Deterrence as the defensive, the minor glyphs** wowtbc; **levels 15 to 60, what to do before Chimera Shot or
Steady Focus is learned, the pet, positioning** neither guide says anything (they are level 90 guides), so those lines come
from the 5.4.8 data and are marked "derived". The guides agree on the order; they differ in two small places: wowtbc puts Rapid
Fire before Serpent Sting and Method after it (I keep wowtbc's, which has the waiting rules), and Method says Haste sits close
to Critical Strike in the stat order (used).

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.3.1 = 000200`
(`conf/playerbots.conf.dist:2717`; one digit per tier, column counted from 0): Posthaste, Binding Shot, Exhilaration, Thrill of
the Hunt, A Murder of Crows, Glaive Toss.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (000200) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Crouching Tiger, Hidden Chimera (Method raid pick) | Posthaste for extra mobility (Method), Narrow Escape | 118675 / 109215 / 109298 | Posthaste. Differs, no loss (passives on Disengage and Deterrence) |
| 2 (30) | Binding Shot (Method), swappable by crowd control need | Wyvern Sting, Intimidation | 109248 / 19386 / 19577 | Binding Shot, same (`MarksmanshipHunterStrategy.cpp:48`, 5.92) |
| 3 (45) | not named for Marksmanship; "stay in Aspect of the Hawk" is on the list, which fits either aspect | Exhilaration, Aspect of the Iron Hawk, Spirit Bond | 109304 / 109260 / 109212 | **Exhilaration, dead:** no action casts it (grep over `src/`, 0 hits) |
| 4 (60) | Fervor: both openers use it and "Fervor on cooldown when Focus is low" is on the list (wowtbc: the Tier 14 pick) | Thrill of the Hunt, "better in heavy AoE" (Method; also its dungeon pick with Barrage), Dire Beast (wowtbc: Tier 15 and later) | 82726 / 109306 / 120679 | Thrill of the Hunt. Differs from the raid pick, same as Method's dungeon swap |
| 5 (75) | A Murder of Crows (both rotations) | Blink Strikes, Lynx Rush | 131894 / 130392 / 120697 | A Murder of Crows, same (default 5.85, `MarksmanshipHunterStrategy.cpp:28`) |
| 6 (90) | Glaive Toss (single target), Barrage for AoE (Method) | Powershot | 117050 / 120360 / 109259 | Glaive Toss, same (default 5.9) |

Notes on the picks (data, not guide):

- Fervor (82726): instant 50 Focus and 5 more per tick for 10 s, 30 s cooldown, no global cooldown (effects of 82726). No action
  exists for it. Thrill of the Hunt (109306): a chance per Focus spender to make the next three Arcane Shots or Multi-Shots 20
  Focus cheaper (effect of 34720); a passive, no code needed. Marksmanship spends Focus on Aimed Shot (50) and Chimera Shot (45)
  more than on Arcane Shot (30), so Thrill helps less here than for Beast Mastery (inference).
- Tier 1, 2, 5 and 6 notes are in `hunter-beastmastery.md` (talents notes): same spells, same code.
- Dire Beast (120679) is wowtbc's Tier 15 pick and has code in the Beast Mastery and Survival defaults, but **not** in the
  Marksmanship defaults (`MarksmanshipHunterStrategy.cpp:26-36`); adding `dire beast` there would make it a no-new-action pick.
- Aspect of the Iron Hawk needs a new action (rotation 8 below); Exhilaration needs one (rotation 9).

Proposed link, guide aligned: **`201000`** (Crouching Tiger / Binding Shot / Aspect of the Iron Hawk / Fervor / A Murder of Crows
/ Glaive Toss); it needs the Iron Hawk and Fervor actions. Without them the safe version is **`202200`** (Spirit Bond, Thrill of
the Hunt: both passives). For a character below 75 only the first four digits are in effect. Owner question 1.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and 75
(config comment `conf/playerbots.conf.dist:2635-2636`). The config order is major, minor, major, minor, minor, major.

| Type | Glyph | Item id | Why (one clause) | Bots today (`42909,43355,42903,45732,43338,42914`) |
|------|-------|--------:|------------------|------------------------|
| Major | Aimed Shot | 42914 | Aimed Shot becomes castable while moving (spell text); Method ranks it first, wowtbc third; bots move with their leader | yes, but in slot 3 (level 75) |
| Major | Animal Bond | 42909 | 10 percent more healing taken while the pet is out | yes (slot 1) |
| Major | Deterrence | 42903 | more damage reduction; both guides; learned at 78 | yes (slot 2, level 50), **no effect until 78** |
| Major, situational | Ice Trap, Liberation, Disengage | 42906, 42899, 42904 | Method's others; Liberation heals 5 percent on Disengage | no |
| Minor | Revive Pet | 43338 | wowtbc; the only minor the bots use | yes (slot 5) |
| Minor | Aspect of the Pack, Aspect of the Cheetah | 43355, 45732 | both guides; unused while the aspects are never cast (see `hunter-beastmastery.md`) | yes (slots 2 and 4) |
| Minor | Direction, Fetch | 87278, 87393 | Method only | no |

Disagreement: Method puts Aimed Shot first and has no Pathfinding; wowtbc has Animal Bond, Deterrence, Aimed Shot, with
Pathfinding for mobility. The premade carries Aimed Shot in the last major slot, so it is missing at levels 25 to 74. Proposed
`PremadeSpecGlyph.3.1`: `42914,43338,42909,43355,45732,42903` = Aimed Shot (25), Revive Pet (25), Animal Bond (50), Aspect of
the Pack (50), Aspect of the Cheetah (75), Deterrence (75).

## Stat order

Method: Hit and Expertise to about 7.5 percent each, Agility, Critical Strike, Haste (close to Critical Strike), Mastery.
wowtbc has no stat order. The caps are level 90 numbers.

For loot rolls and quest rewards below level 90 (derived): a bow, gun or crossbow first (every shot is a percentage of weapon
damage and Aimed Shot is the hardest hitter); then Agility; then Critical Strike, Haste, Mastery. Mastery: Wild Quiver comes
at 80 (data), so Mastery has no effect before that.

Bots today (`Mgr/Item/StatsWeightCalculator.cpp:386-396`): ranged weapon damage 10.0, Agility 2.3, "armor penetration" 2.25,
Hit 2.1, Critical 2.0, Haste 1.8, attack power 1.0, Mastery 0.8, spell power -1. The order agrees with Method (Critical, Haste,
Mastery) and with the derived Mastery rule. Gaps: Expertise has no weight although Method names it; "armor penetration" is a
Wrath stat (inference, harmless); the Careful Aim line (`:702-704`) tests spell 34484, which is not in the data, so it does
nothing (Careful Aim is spell 34483 in 5.4.8 and is not an Intellect conversion).

## What the specialisation knows, by level (5.4.8 data)

The class-wide list (Arcane Shot 1, Steady Shot 3, Concussive Shot 8, Serpent Sting 10, Kill Command 10, Aspect of the Hawk 12,
Hunter's Mark and Disengage 14, Mend Pet 16, Counter Shot 22, Multi-Shot 24, Freezing Trap 28, Feign Death 32, Kill Shot and
Tranquilizing Shot 35, Scare Beast 36, Explosive Trap 38, Trueshot Aura 39 (passive), Ice Trap 46, Trap Launcher 48, Rapid Fire
54, Aspect of the Pack 56, Snake Trap 66, Misdirection 76, Deterrence 78, Camouflage 85, Stampede 87) is in
`hunter-beastmastery.md`. Marksmanship adds, with its costs in Focus (SpellPower):

| Level | Spell (id) |
|------:|------------|
| 1 | Wild Quiver passive (mastery at 80 raises its chance) |
| 10 | Aimed Shot (19434, 50 Focus, 2.5 s cast); the specialisation is chosen here |
| 20 | Careful Aim (34483: Steady Shot and Aimed Shot get +75 percent critical chance on targets above 80 percent health) |
| 30 | Silencing Shot (34490, 24 s, 40 yd, replaces Counter Shot); Concussive Barrage (35102: Chimera Shot and Multi-Shot daze) |
| 45 | Bombardment (35110: a critical Multi-Shot makes the next Multi-Shot cheaper and stronger); tier 3 talent |
| 54 | Rapid Recuperation (53232: Focus every 3 s while Rapid Fire runs, with Rapid Fire learned at 54) |
| 58 | Master Marksman (34487: Steady Shot may stack a buff; at 3 stacks the next Aimed Shot is instant and costs no Focus, aura 82926 for 10 s) |
| 60 | Chimera Shot (53209, 45 Focus, 9 s: refreshes Serpent Sting, heals the hunter; applies Hunter's Mark); tier 4 talent |
| 63 | Steady Focus (53224: two Steady Shots in a row give a haste buff, 53220, and more Focus per Steady Shot) |
| 72 | Piercing Shots (53238: critical Aimed, Steady and Chimera shots leave a bleed) |
| 80 | Mastery: Wild Quiver (76659) |

Marksmanship has **no Cobra Shot** (that is Beast Mastery and Survival at 81); Steady Shot is the filler at every level.

Facts the priority lists rest on: the same as in `hunter-beastmastery.md` (no minimum range, Steady Shot a 2 s cast giving 14
Focus, Hunter's Mark applied by Arcane Shot, Chimera Shot and Kill Command), and:

- Serpent Sting lasts 15 s (spell 118253); Chimera Shot refreshes it, so from 60 the sting needs a cast only to start it.
- Aimed Shot costs 50 Focus and 2.5 s of casting: on trash that dies in 5 to 8 s it is a poor spender; on a fresh target Careful
  Aim makes it the hardest hit (guide rule: Aimed Shot above 80 percent health, Arcane Shot below).

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived"
comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/` at
commit `c0291fd`.

How the bots rank what they do: see `hunter-beastmastery.md` (priority ladder). A Marksmanship hunter gets the `mm` strategy
(`MarksmanshipHunterStrategy`, `Factory/AiFactory.cpp:461-479`) with `cc`, `dps assist`, `aoe`, `bdps` and `boost`. Its default
actions (`MarksmanshipHunterStrategy.cpp:26-36`): Auto Shot 6.0, Stampede 5.95, Glaive Toss 5.9, Murder of Crows 5.85, Kill
Command 5.8, Kill Shot 5.7, Serpent Sting 5.6, Chimera Shot 5.5, Aimed Shot 5.4, Arcane Shot 5.3, Steady Shot 5.2. Edges:
Silencing Shot 40, Binding Shot 5.92, Steady Focus refresh 5.84, Kill Command 18.5, Kill Shot 18, "no stings" 17, sting on an
attacker 16.5 (`:45-63`). An unlearned spell is simply not cast.

### Levels 15 to 29

Known: Aimed Shot (10), Arcane Shot, Steady Shot, Concussive Shot, Serpent Sting, Kill Command, Aspect of the Hawk, Hunter's
Mark, Disengage, Mend Pet, Counter Shot (22), Multi-Shot (24), Freezing Trap (28); passive Careful Aim (20).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat | pet out (Call Pet 1, Revive Pet) and Aspect of the Hawk (12) | yes, see `hunter-beastmastery.md` (pet and aspects) |
| 2 | core | opener on the pull target | Hunter's Mark (1130, 14) | yes (`GenericHunterStrategy.cpp:81`), on every target (see BM sheet, rotation 7) |
| 3 | core | Serpent Sting missing, target lives 8 s or more | Serpent Sting (1978, 10) | yes: 17 (`MarksmanshipHunterStrategy.cpp:59`), default 5.6 |
| 4 | core | target above 80 percent health, Focus 50 or more | Aimed Shot (19434, 10) | partly: default 5.4 (`:33`) with no health condition, a 2.5 s cast on every target |
| 5 | core | Focus 70 or more, otherwise Aimed Shot is not worth it | Arcane Shot (3044, 1) | **wrong:** default 5.3 (`:34`) fires from 30 Focus, below Aimed Shot only because Aimed Shot is ranked above it; when Aimed Shot is not castable (Focus 30 to 49) Arcane Shot spends the Focus before 50 is ever reached |
| 6 | core | otherwise | Steady Shot (56641, 3), 14 Focus | yes, default 5.2 (`:35`) |
| 7 | good | an enemy casts | Counter Shot (147362, 22), Silencing Shot from 30 | yes: `GenericHunterStrategy.cpp:97` and the `mm` edge at 40 (`:45`) |
| 8 | good | two or more enemies in 8 yd of the target | Multi-Shot (2643, 24) in place of Arcane Shot | **wrong order:** at 21.0 above everything (`DpsHunterStrategy.cpp:75`) |
| 9 | good | pet below 65 percent | Mend Pet (136, 16) | yes (`GenericHunterStrategy.cpp:59`) |
| 10 | good | a mob is hitting the hunter | Concussive Shot (5116, 8) | yes (`:83`) |

Kill Command is on the bots' list from level 10 at 18.5 and 5.8, above Aimed Shot, although **neither Marksmanship guide lists
it** (it is a Beast Mastery spell in the data: specialisation 253; the class trainer row nevertheless teaches it to every
hunter at 10, so whether an MM bot can cast it is to check live). A pet that is within 25 yd of the target makes it castable.
It costs 40 Focus, more than an Arcane Shot (30), for pet damage that Marksmanship's own talents do not amplify (derived:
the mastery and talents of this tree boost the hunter's shots, not the pet's).

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 11 | core | the target is at 20 percent health or less | Kill Shot (53351, 35) | yes: edge 18.0 (`:57`) and default 5.7 |
| 12 | good | the target has an enrage or magic effect | Tranquilizing Shot (19801, 35) | yes (`GenericHunterStrategy.cpp:107-110`) |
| 13 | good | two or more attackers on the hunter | Feign Death (5384, 32) | yes (`:57`) |
| 14 | good | an enemy in melee reach | Explosive Trap (13813, 38) | yes at 37 (`:55`), see BM sheet (traps) |
| 15 | good | the leader marked a target | Freezing Trap, Scare Beast | for the marked target only (`cc` strategy) |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 16 | core | pack of three or more, elite or boss | Rapid Fire (3045, 54) on cooldown (180 s); with it Rapid Recuperation gives Focus | **wrong:** only when outmatched two to one (`GenericHunterStrategy.cpp:74`, `:127`, `BoostTrigger`), see BM sheet |
| 17 | core | Master Marksman effect (aura 82926) is up | Aimed Shot (instant, no Focus) first among the spenders (wowtbc: at 3 stacks) | partly: Aimed Shot is cast by its default whenever Chimera Shot is not ready; no condition on the buff, so a free Aimed Shot waits behind Murder of Crows and Glaive Toss from higher tiers |
| 18 | good | two or more enemies | Multi-Shot replaces Arcane Shot (Bombardment 45 makes a critical one cheaper) | see line 8 |
| 19 | good | a tier 3 talent | Iron Hawk or Spirit Bond | no code for Iron Hawk or Exhilaration |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | core | Chimera Shot ready | Chimera Shot (53209, 60), 45 Focus, 9 s: refreshes Serpent Sting and Hunter's Mark | yes, default 5.5 (`:32`), above Aimed Shot |
| 21 | core | Steady Focus buff (53220) missing or under 3 s (guide: under 4 s) | Steady Shot twice in a row (Steady Focus, 63) | yes: edge 5.84 (`:52`, trigger `HunterTriggers.cpp:168`); needs the passive 53224, so it is inert before 63 |
| 22 | core | tier 4 talent | Fervor at low Focus (wowtbc, Method opener) or Thrill of the Hunt (passive) | Thrill (passive); no Fervor action |
| 23 | good | from 66 | Snake Trap before the pull, Explosive Trap in AoE | missing |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | A Murder of Crows on cooldown | default 5.85 above Chimera Shot, with Focus pooling for its 60 cost (`HunterActions.cpp:39`) |
| 76 | Misdirection (not in either guide) | on the main tank when the hunter's threat passes half of the tank's (`GenericHunterStrategy.cpp:89`) |
| 78 | Deterrence at low health | at 45 and 30 percent (`:93`, `:102`) |
| 85, 87 | Camouflage; Stampede on cooldown | Stampede default 5.95 |
| 90 | single target (wowtbc order): Hunter's Mark, Stampede, Rapid Fire, Rabid, Serpent Sting, Fervor at low Focus, Murder of Crows, Dire Beast, Chimera Shot, Steady Shot twice when Steady Focus is under 4 s, Glaive Toss or Barrage, Aimed Shot above 80 percent, Kill Shot, Aimed Shot at 3 Master Marksman stacks, Arcane Shot above 70 Focus, Steady Shot; AoE: Explosive Trap, Glaive Toss, Multi-Shot, Aimed Shot at 3 stacks | Chimera Shot, Steady Focus, Crows, Glaive, Kill Shot present; the Aimed Shot conditions, Fervor, Barrage, Rabid, the 70 Focus gate missing |

## Cooldowns and defensives

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Rapid Fire | 54 (180 s) | on cooldown | at a pack of three or more, an elite or a boss | outmatched only |
| Chimera Shot | 60 (9 s) | on cooldown | on cooldown | yes |
| Silencing Shot | 30 (24 s) | interrupt | any casting enemy | the target (`MarksmanshipHunterStrategy.cpp:45`) |
| Murder of Crows | 75 talent (120 s) | on cooldown | on cooldown | yes |
| Fervor | 60 talent (30 s) | at low Focus | below 30 Focus | missing |
| Feign Death, Disengage, Deterrence | 32, 14, 78 | defensives | see BM sheet | yes |

## Healer triage and mana (healers only)

Not applicable. Focus is the resource.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Chimera Shot (60), Steady Focus (63), Master Marksman (58), Stampede (87), Misdirection (76),
  Deterrence (78) are not all available in the 15 to 60 band; the lists above start without them.
- wowtbc's Marksmanship AoE line "Cobra Shot for Focus" cannot be right: Cobra Shot belongs to Beast Mastery and Survival
  (data: spec 253 and 255 at 81; spec 254 has none).
- wowtbc's "Steady Shot twice if Careful Aim has under 4 s left" refers to the Steady Focus buff (53220); Careful Aim (34483) is a
  permanent passive on targets above 80 percent health.
- Rabid is a pet specialisation spell, and a bot's pet has no specialisation (see `hunter-beastmastery.md`).
- Kill Command is in neither guide's Marksmanship list; the bots cast it (above).
- Hit and Expertise to 7.5 percent is a level 90 number; the "armor penetration" weight is a Wrath stat.
- Both guides' Marksmanship opener pre-casts Aimed Shot (a 2.5 s cast) and uses two Steady Shots; Steady Focus is learned at 63.

## Gap list (ordered by value for levels 15 to 60)

Config (can go at once):

1. Talents: `PremadeSpecLink.3.1` (`conf/playerbots.conf.dist:2717`) to `201000` once the Iron Hawk and Fervor actions exist,
   else `202200`. Today the level 45 pick is a talent with no code.
2. Glyphs: `PremadeSpecGlyph.3.1` (`:2718`) to `42914,43338,42909,43355,45732,42903`. Today the Aimed Shot glyph comes only at
   75 and a level 50 to 77 bot carries Deterrence (nothing before 78) and the unused aspect glyphs.

Rotation (class fixes, wait for the owner's in-game test):

1. **Focus gate and Aimed Shot conditions** (core, biggest for this specialisation): Arcane Shot (`MarksmanshipHunterStrategy.cpp:34`)
   spends Focus from 30, so Aimed Shot (50) and Chimera Shot (45) are reached only by luck (inference: Focus arithmetic; check
   with a combat log). Both guides: Arcane Shot only above 70 Focus; Aimed Shot on targets above 80 percent health, on the
   Master Marksman buff, or when Chimera Shot is on cooldown. Files: `HunterActions.h` (`isUseful` on `CastArcaneShotAction`
   and `CastAimedShotAction`), a new trigger for aura 82926 in `HunterTriggers.*` and an edge in
   `MarksmanshipHunterStrategy::InitTriggers`.
2. **Kill Command out of the Marksmanship lists** (good): edge 18.5 (`:54`) and default 5.8 (`:29`); not in either guide, 40
   Focus. Owner question 3.
3. **Rapid Fire timing** (good): beside the `BoostTrigger` edges (`GenericHunterStrategy.cpp:74`, `:127`) for dungeon packs and
   elites from 54; Rapid Recuperation then returns Focus.
4. **Multi-Shot order** (good): `DpsHunterStrategy.cpp:75` at 21.0 and two enemies should replace Arcane Shot (same Focus gate),
   spam from four (Method and wowtbc).
5. **PvE Disengage and flee** (good): `HunterEnemyTooCloseTrigger` (`HunterTriggers.cpp:202`) fires when the tank holds the target
   and it is in melee reach of the hunter; no minimum range exists. Owner decision (BM sheet, question 4).
6. **Pet and Growl** (good): see `hunter-beastmastery.md` rotation 6; for Marksmanship the pet matters less (no Kill Command in
   the lists), so Growl off behind a tank is the only item.
7. **Hunter's Mark on trash** (good): needLifeTime 0.5 (`HunterTriggers.h:150`); Arcane Shot and Chimera Shot apply the mark.
8. **Aspect of the Iron Hawk** action and trigger, if the guide talent is chosen (`HunterBuffStrategies.cpp`).
9. **Fervor** action (guide talent) and **Exhilaration** (if kept): no actions exist.
10. **Revive Pet and Call Pet in combat**; **Aspect of the Cheetah and Pack** for following; traps (Snake Trap at 66, Explosive
    Trap pre-pull): see the BM sheet, rotation 10, 11 and 13.
11. Stat weights: Expertise weight, remove the dead Careful Aim line (`Mgr/Item/StatsWeightCalculator.cpp:702-704`).
12. Skill tiers: no hunter strategy has tier gating. Lines marked good and expert above are the ones a casual bot should drop.
13. To check live: BT-053 to BT-055 are deployed with their live check pending; that Counter Shot (22) and Silencing Shot (30) are
    known; that an MM bot can cast Kill Command; that the Steady Focus edge (BT history) works once the passive is learned
    at 63.

### Owner questions

1. **Talents.** Guide choice `201000` (Fervor, Aspect of the Iron Hawk; needs two small actions) or bot-friendly `202200`
   (Thrill of the Hunt, Spirit Bond: no code). Tier 15: Crouching Tiger (guide) or Posthaste (today)?
2. **Aimed Shot glyph in the first major slot** (level 25), so the long cast survives the leader walking on. Yes or keep it
   last.
3. **Kill Command for Marksmanship.** Drop it (the guides) or keep it as a spender.
4. **Rapid Fire.** On cooldown at packs of three or more and elites (the guides), or only when outmatched (today).
5. Shared with Beast Mastery (see that sheet): pet family, Growl, PvE Disengage, aspects while following.
