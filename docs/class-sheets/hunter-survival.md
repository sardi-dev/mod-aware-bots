# Hunter: Survival (damage, ranged)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). Trap placement with
  Trap Launcher is left for later. The "bots today" columns below describe the code before the 2026-11-11
  implementation.
- **Sources:**
  - Method, "Survival Hunter Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 23 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the talent hints in the text are usable).
  - wowtbc.gg, "Survival Hunter" in the MoP class guides (MoP Classic, no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellMisc, SpellRange, SpellEffect; `data/db2/Item-sparse.db2` for glyph item ids; the class
    trainer rows of `data/world_db` for the levels spells are taught at) through `spec_data.py` and short read-only scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, not retried), Wowhead (needs a browser).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's
  marks), 61 to 90 briefly from the guides.

Which source was richer: **talent hints (named swaps, the dungeon build), glyph priority, stat order, the opener, the area rule
"more than 4 enemies: spam Multi-Shot"** Method (wowtbc lists every talent option without a pick and has no stat order);
**the Lock and Load rule (Explosive Shot on procs, Ice Trap to make procs), the single target order with Fervor, the 2 to 3
against 4 or more target split, Deterrence as the defensive, the minor glyphs** wowtbc; **levels 15 to 60, what to do before
Black Arrow (50) or Lock and Load (43) exist, the pet, positioning** neither guide says anything (level 90 guides), so those
lines come from the 5.4.8 data and are marked "derived". The two single target lists agree except for small things: Method
puts Murder of Crows before Explosive Shot and Black Arrow, wowtbc puts Explosive Shot first and Kill Shot next; Explosive Shot
(6 s cooldown, the cheapest spender) is the signature of the tree, so I use wowtbc's order.

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.3.2 = 000200`
(`conf/playerbots.conf.dist:2722`; one digit per tier, column counted from 0): Posthaste, Binding Shot, Exhilaration, Thrill of
the Hunt, A Murder of Crows, Glaive Toss.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (000200) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Crouching Tiger, Hidden Chimera (Method raid pick) | Posthaste for extra mobility (Method), Narrow Escape | 118675 / 109215 / 109298 | Posthaste. Differs, no loss (passives on Disengage and Deterrence) |
| 2 (30) | Binding Shot (Method), swappable by crowd control need | Wyvern Sting, Intimidation | 109248 / 19386 / 19577 | Binding Shot, same (`SurvivalHunterStrategy.cpp:89`, 5.97) |
| 3 (45) | not named for Survival | Exhilaration, Aspect of the Iron Hawk, Spirit Bond | 109304 / 109260 / 109212 | **Exhilaration, dead:** no action casts it (grep over `src/`, 0 hits) |
| 4 (60) | Fervor: both openers use it, "Fervor at low Focus" is on wowtbc's list (the Tier 14 pick); Dire Beast (wowtbc: Tier 15 and later) | Thrill of the Hunt "better in heavy AoE" (Method; its dungeon pick with Barrage) | 82726 / 120679 / 109306 | Thrill of the Hunt. Differs from the raid pick, same as Method's dungeon swap |
| 5 (75) | A Murder of Crows (both rotations) | Blink Strikes, Lynx Rush | 131894 / 130392 / 120697 | A Murder of Crows, same (default 5.94, `SurvivalHunterStrategy.cpp:65`) |
| 6 (90) | Glaive Toss (single target), Barrage for AoE and dungeons (Method) | Powershot | 117050 / 120360 / 109259 | Glaive Toss, same (default 5.96) |

Notes on the picks (data, not guide):

- Survival's Focus goes mostly to Explosive Shot (25), Black Arrow (35) and Murder of Crows (60); Arcane Shot and Multi-Shot are
  the minor spenders that Thrill of the Hunt cheapens, so Thrill is worth less here than the AoE swap suggests (inference).
- **Dire Beast (120679) is the pick that needs no new code:** it is already in the Survival defaults (5.92,
  `SurvivalHunterStrategy.cpp:66`) and is wowtbc's Tier 15 and later talent; Fervor would need a new action.
- Fervor (82726): instant 50 Focus and 5 more per tick for 10 s, 30 s, no global cooldown. A Murder of Crows: 60 Focus, 120 s.
- Tier 1, 2 and 6 notes are in `hunter-beastmastery.md` (talents notes); Aspect of the Iron Hawk needs a new action (rotation
  8 below); Exhilaration needs one (rotation 9).

Proposed link, guide aligned: **`201000`** (Crouching Tiger / Binding Shot / Aspect of the Iron Hawk / Fervor / A Murder of
Crows / Glaive Toss) needs the Iron Hawk and Fervor actions. Without them the safe version is **`202100`** (Spirit Bond: a
passive; Dire Beast: wowtbc's pick, already coded). For a character below 75 only the first four digits are in effect.
Owner question 1.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and 75
(config comment `conf/playerbots.conf.dist:2635-2636`). The config order is major, minor, major, minor, minor, major.

| Type | Glyph | Item id | Why (one clause) | Bots today (`42909,43355,42903,45732,43338,42911`) |
|------|-------|--------:|------------------|------------------------|
| Major | Animal Bond | 42909 | 10 percent more healing taken while the pet is out; both guides | yes (slot 1) |
| Major | Deterrence | 42903 | more damage reduction; both guides' first or second; learned at 78 | yes (slot 2, level 50), **no effect until 78** |
| Major | Pathfinding | 42911 | wowtbc: faster Aspect of the Cheetah and Pack; the bots never cast either aspect | yes (slot 3), **no effect** |
| Major, derived | Mending | 42900 | Mend Pet ticks every second and heals more; the bots cast Mend Pet at 65 percent pet health | no |
| Major, situational | Disengage, Ice Trap, Liberation | 42904, 42906, 42899 | Method (Disengage first, the other two as options); Ice Trap's wider slow suits Lock and Load use of Ice Trap, which no bot does | no |
| Minor | Revive Pet | 43338 | wowtbc; the only minor the bots use | yes (slot 5) |
| Minor | Aspect of the Pack, Aspect of the Cheetah | 43355, 45732 | both guides; unused while the aspects are never cast | yes (slots 2 and 4) |
| Minor | Direction, Fetch | 87278, 87393 | Method only | no |

Disagreement: Method's majors are Deterrence, Animal Bond, Disengage (Ice Trap, Liberation as options); wowtbc's are Animal
Bond, Deterrence, Pathfinding (Disengage with Posthaste). The premade follows wowtbc. Survival has no glyph of its own in either
guide. Proposed `PremadeSpecGlyph.3.2`: `42909,43338,42900,43355,45732,42903` = Animal Bond (25), Revive Pet (25), Mending (50),
Aspect of the Pack (50), Aspect of the Cheetah (75), Deterrence (75).

## Stat order

Method: Hit and Expertise to about 7.5 percent each, Agility, Critical Strike, Mastery, Haste. wowtbc has no stat order. The caps
are level 90 numbers.

For loot rolls and quest rewards below level 90 (derived): a bow, gun or crossbow first; then Agility; then Critical Strike. Mastery:
Essence of the Viper (76658, level 80) raises all magical damage, which is most of Survival's damage (Explosive Shot is fire,
Black Arrow shadow, Serpent Sting nature, Arcane Shot arcane), but it does not exist before 80, so below 80 Mastery is worth
nothing and Haste (ranged haste speeds the 2 s Steady Shot and the Focus income) ranks above it.

Bots today (`Mgr/Item/StatsWeightCalculator.cpp:373-384`, shared with Beast Mastery): ranged weapon damage 7.5, Agility 2.5, Hit
1.7, Haste 1.6, Mastery 1.5, Critical 1.4, attack power 1.0, "armor penetration" 1.5, spell power -1. Against the guide:
**Critical is the lowest of the four secondaries** where Method has it third; Mastery above Critical is wrong before 80;
Expertise has no weight; the Careful Aim and Hunter vs. Wild lines (`:702-707`) test spell ids that do nothing in 5.4.8.

## What the specialisation knows, by level (5.4.8 data)

The class-wide list is in `hunter-beastmastery.md` (Arcane Shot 1, Steady Shot 3, Concussive Shot 8, Serpent Sting 10, Kill
Command 10, Aspect of the Hawk 12, Hunter's Mark and Disengage 14, Mend Pet 16, Counter Shot 22, Multi-Shot 24, Freezing Trap
28, Feign Death 32, Kill Shot and Tranquilizing Shot 35, Scare Beast 36, Explosive Trap 38, Trueshot Aura 39 (passive), Ice Trap
46, Trap Launcher 48, Rapid Fire 54, Aspect of the Pack 56, Snake Trap 66, Misdirection 76, Deterrence 78, Camouflage 85,
Stampede 87). Survival adds, with its costs in Focus (SpellPower):

| Level | Spell (id) |
|------:|------------|
| 10 | Explosive Shot (53301, 25 Focus, 6 s, 40 yd: fire damage now and every second for 2 s; applies Hunter's Mark); the specialisation is chosen here |
| 43 | Lock and Load (56343: a chance when a Freezing or Ice Trap catches an enemy, and a chance on Black Arrow damage, to make the next two Explosive Shots cost no Focus and start no cooldown) |
| 50 | Black Arrow (3674, 35 Focus, 30 s, a 20 s shadow damage over time) |
| 55 | Entrapment (19387: Ice Trap and Snake Trap root the enemies they catch) |
| 63 | Viper Venom (118976: 3 Focus every time Serpent Sting deals damage, at most every 3 s) |
| 64 | Trap Mastery (63458: traps and Black Arrow lose 6 s of cooldown; Explosive Trap and Black Arrow damage up) |
| 68 | Serpent Spread (87935: Multi-Shot also applies your Serpent Sting) |
| 70 | Improved Serpent Sting (82834: more damage and an instant hit) |
| 80 | Mastery: Essence of the Viper (76658) |
| 81 | Cobra Shot (77767, replaces Steady Shot: 14 Focus, lengthens Serpent Sting) |

Before 81 the filler is Steady Shot. Survival has no Beast Cleave (data: Beast Mastery only, specialisation 253) and no Aimed
Shot or Chimera Shot.

Facts the priority lists rest on: the same as in `hunter-beastmastery.md` (no minimum range, Steady Shot a 2 s cast giving 14
Focus, Hunter's Mark applied by Arcane Shot, Kill Command and Explosive Shot), and:

- Explosive Shot has a 6 s cooldown and costs 25 Focus: it is the cheapest, most frequent spender, and a Lock and Load charge
  makes it free and cooldown-free, so on a proc it is cast again at once.
- Serpent Sting lasts 15 s; from 63 each tick returns Focus (Viper Venom), so the sting always stays up.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived"
comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/` at
commit `c0291fd`.

How the bots rank what they do: see `hunter-beastmastery.md` (priority ladder). A Survival hunter gets the `surv` strategy
(`SurvivalHunterStrategy`, `Factory/AiFactory.cpp:461-479`) with `cc`, `dps assist`, `aoe`, `bdps` and `boost`. Its default
actions (`SurvivalHunterStrategy.cpp:63-76`): Auto Shot 6.0, Stampede 5.98, Glaive Toss 5.96, Murder of Crows 5.94, Dire Beast
5.92, Kill Command 5.9, Kill Shot 5.8, Explosive Shot 5.7, Black Arrow 5.6, Serpent Sting 5.5, Arcane Shot 5.3, Cobra Shot 5.25,
Steady Shot 5.2. Edges (`:86-104`): Lock and Load -> Explosive Shot 28, Binding Shot 5.97, Kill Command 18.5, Kill Shot 18,
"no Explosive Shot on the target" 17.5, "no Black Arrow on the target" 16.5, "no stings" 15.5, sting on an attacker 15. An
unlearned spell is simply not cast.

### Levels 15 to 29

Known: Explosive Shot (10), Arcane Shot, Steady Shot, Concussive Shot, Serpent Sting, Kill Command, Aspect of the Hawk, Hunter's
Mark, Disengage, Mend Pet, Counter Shot (22), Multi-Shot (24), Freezing Trap (28). No Black Arrow (50), no Lock and Load (43).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat | pet out (Call Pet 1, Revive Pet) and Aspect of the Hawk (12) | yes, see `hunter-beastmastery.md` |
| 2 | core | opener on the pull target | Hunter's Mark (1130, 14) | yes (`GenericHunterStrategy.cpp:81`), on every target (BM sheet, rotation 7) |
| 3 | core | Serpent Sting missing, target lives 8 s or more | Serpent Sting (1978, 10) | yes: edge 15.5 (`SurvivalHunterStrategy.cpp:100`), default 5.5; sits **below** Explosive Shot and Black Arrow although both guides keep the sting up first |
| 4 | core | Explosive Shot not on the target and ready | Explosive Shot (53301, 10) on cooldown (6 s) | yes: edge 17.5 (`:95`) and default 5.7 (`:69`); the cast is refused while the shot's own 2 s damage is on the target (`HunterActions.cpp` `isUseful`) |
| 5 | core | Focus 70 or more | Arcane Shot (3044, 1) as the spender (both guides: only above 70 Focus) | **wrong:** default 5.3 (`:73`) fires from 30 Focus, which starves Explosive Shot (25 is cheaper, so less here), Black Arrow (35, from 50), Kill Command (40) and Murder of Crows (60) (inference, to check in a combat log) |
| 6 | core | otherwise | Steady Shot (56641, 3), 14 Focus; Cobra Shot only from 81 | yes, default 5.2 (`:75`); Cobra Shot 5.25 is inert before 81 |
| 7 | good | an enemy casts | Counter Shot (147362, 22) | yes (`GenericHunterStrategy.cpp:97`) |
| 8 | good | two or more enemies in 8 yd of the target | Multi-Shot (2643, 24) in place of Arcane Shot (both guides: 2 to 3 targets) | **wrong order:** `DpsHunterStrategy.cpp:75`, 21.0, above Explosive Shot and every default, from 2 enemies |
| 9 | good | pet below 65 percent | Mend Pet (136, 16) | yes (`GenericHunterStrategy.cpp:59`) |
| 10 | good | a mob is hitting the hunter | Concussive Shot (5116, 8) | yes (`:83`) |

Kill Command is on the bots' list from level 10 at 18.5 (`:91`) and 5.9, above Explosive Shot (17.5 and 5.7), although **neither
Survival guide lists it** (it is a Beast Mastery spell in the data: specialisation 253; the class trainer row nevertheless
teaches it to every hunter at 10, so whether an SV bot can cast it is to check live). At 40 Focus it costs more than
Explosive Shot (25) and more than Arcane Shot (30), for pet damage Survival's tree does not amplify (derived).

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 11 | core | the target is at 20 percent health or less | Kill Shot (53351, 35) (wowtbc: right after Explosive Shot) | yes: edge 18.0 (`:93`) and default 5.8, above Explosive Shot's 17.5 and 5.7 |
| 12 | core | Lock and Load is up (43) | Explosive Shot (free, no cooldown) again at once | yes in form: "lock and load" -> Explosive Shot at 28 (`:86-87`); the chain "rank 4 -> 3 -> 2 -> 1" in `SurvivalHunterStrategy.cpp:22-44` all casts 53301 (`HunterActions.cpp`). The trigger tests the aura by name (`HunterTriggers.cpp` `LockAndLoadTrigger`), and the passive talent 56343 has the same name as the proc buff 56453, so it may be true whenever the talent is known (inference, to check live) |
| 13 | good | the target has an enrage or magic effect | Tranquilizing Shot (19801, 35) | yes (`GenericHunterStrategy.cpp:107-110`) |
| 14 | good | two or more attackers on the hunter | Feign Death (5384, 32) | yes (`:57`) |
| 15 | good | an enemy in melee reach | Explosive Trap (13813, 38): both guides use it on cooldown at 2 or more targets | yes at 37 (`:55`) but only for an enemy already in melee reach, see BM sheet (traps) |
| 16 | expert | an add that can be rooted, Lock and Load not on its internal cooldown | Ice Trap (13809, 46): a trap hit can make a Lock and Load proc (wowtbc) | missing (no action for Ice Trap) |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 17 | core | Black Arrow not on the target and ready | Black Arrow (3674, 50) on cooldown, 35 Focus (a Black Arrow tick can also make a Lock and Load proc) | yes: edge 16.5 (`:97`) and default 5.6 |
| 18 | core | pack of three or more, elite or boss | Rapid Fire (3045, 54) on cooldown (180 s) | **wrong:** only when outmatched two to one (`GenericHunterStrategy.cpp:74`, `:127`, `BoostTrigger`), see BM sheet |
| 19 | good | a tier 3 talent | Iron Hawk or Spirit Bond | no code for Iron Hawk or Exhilaration |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | core | tier 4 talent | Fervor at low Focus (50 Focus back at once), or Dire Beast on cooldown, or Thrill of the Hunt | Thrill (passive); Dire Beast is coded (5.92); no Fervor action |
| 21 | good | from 64 | Trap Mastery shortens Black Arrow and the traps by 6 s: Black Arrow every 24 s | passive; no code needed (the Black Arrow edge waits for the 20 s dot to end and the cast check for the cooldown) |
| 22 | good | from 68, two or more enemies | Multi-Shot spreads Serpent Sting (Serpent Spread) | Serpent Spread needs no code; "sting on an attacker" (`:103`, 15.0) also casts the sting per mob at 15 Focus each |
| 23 | expert | from 66 | Snake Trap before the pull | missing |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | A Murder of Crows on cooldown | default 5.94 with Focus pooling for its 60 cost (`HunterActions.cpp:39`) |
| 76 | Misdirection (not in either guide) | on the main tank when the hunter's threat passes half of the tank's (`GenericHunterStrategy.cpp:89`) |
| 78 | Deterrence at low health | at 45 and 30 percent (`:93`, `:102`) |
| 81 | Cobra Shot as the filler, refreshing Serpent Sting | default 5.25 above Steady Shot |
| 85, 87 | Camouflage; Stampede on cooldown | Stampede default 5.98 |
| 90 | single target (wowtbc order): Hunter's Mark, Stampede, Rapid Fire, Rabid, Fervor at low Focus, Explosive Shot, Kill Shot, Murder of Crows, Black Arrow, Dire Beast, Glaive Toss or Barrage, Serpent Sting, Arcane Shot above 70 Focus, Ice Trap on rootable adds, Cobra Shot; AoE: Explosive Trap, Glaive Toss or Barrage, Multi-Shot, Explosive Shot on Lock and Load, Cobra Shot | Explosive Shot, Lock and Load, Black Arrow, Crows, Glaive, Kill Shot present; Fervor, Barrage, Ice Trap, Rabid, the 70 Focus gate and the Rapid Fire timing missing |

## Cooldowns and defensives

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Explosive Shot | 10 (6 s) | on cooldown, on every Lock and Load charge | same | yes |
| Black Arrow | 50 (30 s) | on cooldown | same | yes |
| Rapid Fire | 54 (180 s) | on cooldown | at a pack of three or more, an elite or a boss | outmatched only |
| Murder of Crows | 75 talent (120 s) | on cooldown | on cooldown | yes |
| Fervor | 60 talent (30 s) | at low Focus | below 30 Focus | missing |
| Explosive Trap, Ice Trap | 38, 46 (30 s shared) | AoE on cooldown; Ice Trap for Lock and Load | on cooldown at packs | Explosive Trap only on a melee enemy; Ice Trap missing |
| Feign Death, Disengage, Deterrence | 32, 14, 78 | defensives | see BM sheet | yes |

## Healer triage and mana (healers only)

Not applicable. Focus is the resource.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Black Arrow (50), Lock and Load (43), Cobra Shot (81), Stampede (87), Misdirection (76), Deterrence
  (78) are not all available in the 15 to 60 band; the lists above start without them. Explosive Shot (10) is.
- Cobra Shot is the filler of the guides; before 81 the filler is Steady Shot.
- Method's Survival page mentions Beast Cleave; in the data it belongs to Beast Mastery only, so Survival's Multi-Shot gives the
  pet no cleave.
- Rabid is a pet specialisation spell, and a bot's pet has no specialisation (see `hunter-beastmastery.md`).
- Kill Command is in neither guide's Survival list; the bots cast it first (above).
- wowtbc's Ice Trap line assumes Lock and Load and the trap together; Ice Trap is learned at 46 and Lock and Load at 43, so
  the pairing starts at 46.
- Hit and Expertise to 7.5 percent is a level 90 number; the "armor penetration" weight is a Wrath stat.

## Gap list (ordered by value for levels 15 to 60)

Config (can go at once):

1. Talents: `PremadeSpecLink.3.2` (`conf/playerbots.conf.dist:2722`) to `202100` now (Spirit Bond: no code; Dire Beast: coded and
   wowtbc's pick), or `201000` once the Iron Hawk and Fervor actions exist. Today the level 45 pick is a talent with no code.
2. Glyphs: `PremadeSpecGlyph.3.2` (`:2723`) to `42909,43338,42900,43355,45732,42903`. Today a level 50 to 77 Survival bot carries
   Deterrence (nothing before 78), Pathfinding and unused aspect glyphs.

Rotation (class fixes, wait for the owner's in-game test):

1. **Kill Command out of the Survival lists** (core, biggest for this specialisation): the edge 18.5 (`SurvivalHunterStrategy.cpp:91`)
   and default 5.9 (`:67`) rank it above Explosive Shot, Black Arrow and Kill Shot's cheaper turns, and it is in neither
   guide. Owner question 3.
2. **Arcane Shot Focus gate** (core): both guides spend Arcane Shot only above 70 Focus; `:73` casts it from 30 (inference: Focus
   arithmetic; check with a combat log). Fix: an `isUseful` on `CastArcaneShotAction` (`HunterActions.h`).
3. **Multi-Shot order** (good): `DpsHunterStrategy.cpp:75` at 21.0 from two enemies puts Multi-Shot above Explosive Shot and
   Black Arrow; the guides replace Arcane Shot at 2 to 3 targets and spam Multi-Shot from four.
4. **Rapid Fire timing** (good): beside the `BoostTrigger` edges (`GenericHunterStrategy.cpp:74`, `:127`) for dungeon packs and
   elites from 54.
5. **Serpent Sting priority** (good): edge 15.5 (`:100`) below Explosive Shot (17.5) and Black Arrow (16.5); both guides put it
   second; at 15 s long it costs 15 Focus once per target and Viper Venom (63) pays it back.
6. **PvE Disengage and flee** (good): `HunterEnemyTooCloseTrigger` (`HunterTriggers.cpp:202`) fires when the tank holds the target
   and it is in melee reach of the hunter; no minimum range exists. Owner decision (BM sheet, question 4).
7. **Hunter's Mark on trash** (good): needLifeTime 0.5 (`HunterTriggers.h:150`); Explosive Shot and Arcane Shot apply the mark.
8. **Aspect of the Iron Hawk** action and trigger, if the guide talent is chosen (`HunterBuffStrategies.cpp`).
9. **Fervor** action (guide talent) and **Exhilaration** (if kept): no actions exist.
10. **Traps** (expert): Ice Trap for Lock and Load, Explosive Trap pre-pull and on cooldown in AoE (needs the Trap Launcher
    toggle), Snake Trap (66). Pet items (Growl off behind a tank, Revive Pet in combat) and the aspects while following: BM sheet.
11. Stat weights: Mastery below Haste and Critical before 80, Expertise weight, remove the dead Careful Aim and Hunter vs. Wild
    lines (`Mgr/Item/StatsWeightCalculator.cpp:373-384`, `:702-707`).
12. Skill tiers: no hunter strategy has tier gating. Lines marked good and expert above are the ones a casual bot should drop.
13. To check live: BT-053 to BT-055 are deployed with their live check pending; that Counter Shot (22) is known; that an SV
    bot can cast Kill Command; whether the Lock and Load edge at `:86` fires on the proc or on the talent itself (the trigger matches the aura by
    name, see line 12); what a bot pet's spell list holds.

### Owner questions

1. **Talents.** Guide choice `201000` (Fervor, Aspect of the Iron Hawk; two small actions) or bot-friendly `202100` (Spirit Bond,
   Dire Beast; no code). Tier 15: Crouching Tiger (guide) or Posthaste (today)?
2. **Glyphs for 50 to 77.** Mending (derived, used by the bots' Mend Pet) or Method's Disengage or Liberation.
3. **Kill Command for Survival.** Drop it (both guides) or keep it as a spender.
4. **Rapid Fire.** On cooldown at packs of three or more and elites (the guides), or only when outmatched (today).
5. Shared with Beast Mastery (see that sheet): pet family, Growl, PvE Disengage, aspects while following.

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 5 players of this specialisation (rating 2471 to 2714). 2v2: 4 players (rating 2776 to 2932). Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** 3v3 (5 players)
- **BT-324 result:** updated (BT-450); 2v2 differs on two glyphs (BT-454)
- **Set now:** `PremadeSpecLinkPvp.3.2 = 012000` (was `002000`), `PremadeSpecGlyphPvp.3.2 = 42917,45732,42908,43338,43355,42909`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Posthaste 100% / 100% / 9 of 9 | Posthaste | Posthaste | matches |
| 2 (30) | Wyvern Sting 100% / 75% / 8 of 9, Binding Shot 0% / 25% / 1 of 9 | Binding Shot | Wyvern Sting | changed (5 of 5) |
| 3 (45) | Spirit Bond 100% / 100% / 9 of 9 | Spirit Bond | Spirit Bond | matches |
| 4 (60) | Fervor 100% / 100% / 9 of 9 | Fervor | Fervor | matches |
| 5 (75) | A Murder of Crows 100% / 100% / 9 of 9 | A Murder of Crows | A Murder of Crows | matches |
| 6 (90) | Glaive Toss 100% / 100% / 9 of 9 | Glaive Toss | Glaive Toss | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Animal Bond (42909) | 100% | 100% | 9 of 9 | yes | yes |
| major | Solace (42917) | 100% | 100% | 9 of 9 | yes | yes |
| major | Explosive Trap (42908) | 80% | 25% | 5 of 9 | yes | yes |
| major | Black Ice (85684) | 20% | 75% | 4 of 9 | no | no |
| minor | Revive Pet (43338) | 100% | 100% | 9 of 9 | yes | yes |
| minor | Aspect of the Cheetah (45732) | 80% | 100% | 8 of 9 | yes | yes |
| minor | Stampede (43356) | 40% | 75% | 5 of 9 | no | no |
| minor | Aspect of the Pack (43355) | 0% | 0% | 0 of 9 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **What changed (3v3, 5 players):** tier 30 is Wyvern Sting (100 percent in 3v3, 75 in 2v2, pooled 8 of 9) instead of Binding Shot. The other five tiers match in both brackets (the set is the marksmanship guide's). Five players is the smallest 3v3 sample that was changed.
- **2v2 disagrees (BT-454), 3v3 pick kept:** the four 2v2 hunters hold the major Black Ice (75 percent; 3v3 20) rather than Explosive Trap (25; 3v3 80), and the minor Stampede (75; 3v3 40) rather than Aspect of the Pack (the set holds it, nobody on the ladder does). Pooled both stay under 60 (Explosive Trap 5 of 9, Black Ice 4 of 9, Stampede 5 of 9), so nothing moved.
- **Check live:** no strategy rule names Wyvern Sting; the 1v1 engine row `OneVsOnePolicy.h:437` casts it as a root-kind spell, whether the battleground lane does is to check.
