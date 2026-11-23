# Hunter: Beast Mastery (damage, ranged, pet)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). Trap placement with
  Trap Launcher is left for later. The "bots today" columns below describe the code before the 2026-11-11
  implementation.
- **Sources:**
  - Method, "Beast Mastery Hunter Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 23 July 2025),
    read 2026-11-11 through a scripted fetch (the talent build is an image: only the talent hints in the text are usable).
  - wowtbc.gg, "Beast Mastery Hunter" in the MoP class guides (MoP Classic, no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellMisc, SpellRange, SpellEffect, ChrSpecialization; `data/db2/Item-sparse.db2` for glyph item
    ids; the class trainer rows of `data/world_db` for the levels spells are taught at) through `spec_data.py` and short
    read-only scripts; the core source (the SkyFire core) for the pet code.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, not retried), Wowhead (needs a browser).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's
  marks), 61 to 90 briefly from the guides.

Which source was richer: **talent picks (named picks and the dungeon swap), glyph priority, stat order, opener, the AoE
swap rule** Method (wowtbc lists every talent option without a pick and has no stat order); **the single target order with
its conditions (Dire Beast and Focus Fire may wait for Bestial Wrath), the 2 to 3 against 4 or more target split, all talent
options per tier, the minor glyphs, Deterrence as the defensive** wowtbc; **levels 15 to 60, what to do before a spell is
learned, the pet, Mend Pet, Misdirection, positioning** neither guide says anything useful (both are level 90 raid guides and
say outright that the pet family, Mend Pet, Misdirection and Growl are not covered), so those lines come from the 5.4.8 data
and are marked "derived". The two guides agree on the single target order up to small details (Method puts Serpent Sting
second and Hunter's Mark first, wowtbc puts Bestial Wrath fourth and Stampede third); I use wowtbc's list because it has the
waiting rules, and Method's talent picks.

## Talents

Tiers and columns are from Talent.dbc (the three hunter specialisations share one list). "Bots today" is
`AiPlayerbot.PremadeSpecLink.3.0 = 000100` (`conf/playerbots.conf.dist:2712`; one digit per tier, column counted from 0):
Posthaste, Binding Shot, Exhilaration, Dire Beast, A Murder of Crows, Glaive Toss.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (000100) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Crouching Tiger, Hidden Chimera (Method raid pick) | Posthaste for snares and mobility (Method), Narrow Escape | 118675 / 109215 / 109298 | Posthaste. Differs, no loss: all three are passives on Disengage and Deterrence |
| 2 (30) | Binding Shot (Method) | Wyvern Sting, Intimidation (wowtbc lists them) | 109248 / 19386 / 19577 | Binding Shot, same (`BeastMasteryHunterStrategy.cpp:57`, cast whenever the target is 15 to 30 yd away) |
| 3 (45) | Aspect of the Iron Hawk (Method) | Exhilaration for burst healing (Method), Spirit Bond | 109260 / 109304 / 109212 | **Exhilaration. Differs, and dead:** no action casts it (grep over `src/`, 0 hits) |
| 4 (60) | Dire Beast (Method) | Thrill of the Hunt for cleave, Fervor (wowtbc: the Tier 14 pick) | 120679 / 109306 / 82726 | Dire Beast, same (default action 5.65, `BeastMasteryHunterStrategy.cpp:38`) |
| 5 (75) | Neither guide's BM rotation uses a tier 75 talent; wowtbc lists Murder of Crows, Blink Strikes, Lynx Rush | | 131894 / 130392 / 120697 | A Murder of Crows (derived keep: it has code, `HunterActions.cpp:39`; the other two have none) |
| 6 (90) | Glaive Toss (Method) | Barrage for many adds; Powershot | 117050 / 120360 / 109259 | Glaive Toss, same (default 5.75) |

Notes on the picks (data, not guide):

- Crouching Tiger, Posthaste and Narrow Escape all hang on Disengage (spell texts). Crouching Tiger takes 10 s off Disengage
  and, through spell 148467, 60 s off Deterrence (effect values of 118675); Posthaste gives 60 percent movement speed after
  Disengage (effect of 118922); Narrow Escape webs enemies around the landing spot.
- Aspect of the Iron Hawk (109260) is its own spell: +25 percent ranged attack power like Aspect of the Hawk and 10 percent
  less damage taken (effects), "replaces Aspect of the Hawk" (text). The bots cast Aspect of the Hawk by name
  (`HunterBuffStrategies.cpp:17`), so the pick needs one new action (gap list, rotation 9).
- Exhilaration (109304): instant, heals the hunter for 30 percent and the pet (effect), 120 s. Spirit Bond (109212): a passive
  that heals hunter and pet while the pet is out; no code needed.
- Murder of Crows (131894): 60 Focus, 120 s, a 30 s damage over time; the cooldown shrinks on a target under 20 percent. Glaive
  Toss is level 90 only.
- Binding Shot as a default action fires on every pull at range, and the tether stuns an enemy that moves 5 yd or more away from
  the arrow (text). On a mob the tank is moving this can stun it for a moment; inference, to check live.

Proposed link, guide aligned: **`201100`** (Crouching Tiger / Binding Shot / Aspect of the Iron Hawk / Dire Beast / A Murder of
Crows / Glaive Toss); it needs the Iron Hawk action (rotation 9). Without that code the safe version is **`202100`** (Spirit
Bond at tier 45: no code, a real effect). For a character below 75 only the first four digits are in effect. The first two
digits are an owner choice (owner question 1).

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and 75
(config comment `conf/playerbots.conf.dist:2635-2636`). The config order is major, minor, major, minor, minor, major; a glyph
goes to the first free slot of its type.

| Type | Glyph | Item id | Why (one clause) | Bots today (`42909,43355,42903,45732,43338,42911`) |
|------|-------|--------:|------------------|------------------------|
| Major | Animal Bond | 42909 | 10 percent more healing taken while the pet is out (effect of 24529); both guides rank it first or second | yes (slot 1, level 25) |
| Major | Deterrence | 42903 | more damage reduction from Deterrence; both guides, but Deterrence is learned at 78 | yes (slot 2, level 50), **no effect until 78** |
| Major | Pathfinding | 42911 | wowtbc: faster Aspect of the Cheetah and Pack, faster mount; the bots never cast either aspect (see "Aspects") | yes (slot 3, level 75), **no effect** |
| Major, derived | Mending | 42900 | Mend Pet ticks every second and heals more (text); the bots cast Mend Pet below 65 percent pet health (`GenericHunterStrategy.cpp:59`), so it is used | no (in neither guide) |
| Major, situational | Disengage, Liberation, Endless Wrath | 42904, 42899, 42902 | Method: Disengage; Liberation heals 5 percent on Disengage; Endless Wrath: the pet cannot die while Bestial Wrath runs (10 s per minute) | no |
| Minor | Revive Pet | 43338 | wowtbc; the only minor the bots actually use (they revive the pet out of combat) | yes (slot 5, level 75) |
| Minor | Aspect of the Pack, Aspect of the Cheetah | 43355, 45732 | both guides; the Cheetah glyph turns the daze into a 5 s aspect cooldown; unused while the aspects are never cast | yes (slots 2 and 4) |
| Minor | Direction, Fetch | 87278, 87393 | Method only; the bots use neither Misdirection markers nor Fetch | no |

Disagreement: Method's majors are Deterrence, Animal Bond, Disengage; wowtbc's are Animal Bond, Deterrence, Pathfinding
(Disengage with Posthaste). The premade follows wowtbc. Method is richer on priority, wowtbc on the minors; the data decides
what is major or minor. The slot order is derived: a glyph must be useful at the level its slot unlocks. Proposed
`PremadeSpecGlyph.3.0`: `42909,43338,42900,43355,45732,42903` = Animal Bond (25), Revive Pet (25), Mending (50), Aspect of the
Pack (50), Aspect of the Cheetah (75), Deterrence (75).

## Stat order

Method: Hit and Expertise to about 7.5 percent each, Agility, Critical Strike, Mastery, Haste. wowtbc has no stat order. The caps
are level 90 numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **A bow, gun or crossbow first.** Every shot is a percentage of weapon damage (spell texts of Arcane Shot, Steady Shot,
  Cobra Shot) and Auto Shot fires from the ranged slot, so ranged weapon damage outweighs any stat. Melee weapons only carry
  stats for a hunter (Method: melee weapons removed).
- Then Agility (attack power and critical strike), then the secondary order of the guide. Mail from level 40 (class trainer
  teaches Mail at 40). Hit and Expertise matter only near level 90.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:373-384`, shared by Beast Mastery and Survival): ranged weapon damage 7.5,
  Agility 2.5, Hit 1.7, Haste 1.6, Mastery 1.5, Critical 1.4, attack power 1.0, "armor penetration" 1.5, spell power -1.
  Against the guide: **Haste above Mastery above Critical** where Method has Critical, Mastery, Haste; **Expertise has no weight**
  although Method names it with Hit. The Careful Aim and Hunter vs. Wild lines (`:702-707`) test spell ids that are not
  hunter talents in 5.4.8 (34484 is not in the data; 56341 is a Wrath rank) and so do nothing (inference from the data, to
  check). A slow ranged weapon preference exists behind `PreferredSpecWeapons` (default off, `:1055-1061`); it is a Wrath rule
  and unproven here.

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts; the data holds many old rows of the same name. Levels are the class trainer's
(`npc_trainer` entry 200003) where it teaches the spell, otherwise the specialisation or skill line data. Costs are Focus
(SpellPower); the global cooldown is 1 s (SpellCooldowns). Maximum Focus is 100 (Kindred Spirits, level 58, adds 20).

| Level | Spell (id) |
|------:|------------|
| 1 | Arcane Shot (3044, 30 Focus, instant), Auto Shot (75), Call Pet 1 (883), Revive Pet (982, 35 Focus, 4 s cast), Wild Quiver passive |
| 3 | Steady Shot (56641, 2 s cast, gives 14 Focus) |
| 4 | Track Beasts (1494); Track Humanoids (19883) is taught at 12 |
| 8 | Concussive Shot (5116, 5 s, slow) |
| 10 | Serpent Sting (1978, 15 Focus); Kill Command (34026, 40 Focus, 6 s, 100 yd, the pet must be within 25 yd of the target); Tame Beast, Beast Lore, Dismiss Pet; the specialisation is chosen here |
| 12 | Aspect of the Hawk (13165, +25 percent ranged attack power) |
| 14 | Hunter's Mark (1130, 5 min); Disengage (781, 20 s) |
| 15 | Scatter Shot (19503, 30 s); tier 1 talent |
| 16 | Mend Pet (136, no cost, 10 s, 5 percent of pet health every 2 s); Aspect of the Cheetah (5118; the spell data says 24, the trainer row 16); Eagle Eye |
| 18 | Call Pet 2 (83242) |
| 20 | Go for the Throat (34954: Auto Shot crits give the pet 15 Focus) |
| 22 | Counter Shot (147362, 24 s, 40 yd; learned with the class skill line, not a trainer row: to check live that bots know it) |
| 24 | Multi-Shot (2643, 40 Focus); Beast Cleave (115939: after Multi-Shot the pet's attacks hit all nearby enemies for 75 percent for 4 s) |
| 28 | Freezing Trap (1499, 30 s shared trap cooldown) |
| 30 | Frenzy (19623: the pet gains attack speed stacks up to 5); tier 2 talent |
| 32 | Focus Fire (82692, no cost: spends the pet's Frenzy stacks for 6 percent ranged haste each, 20 s); Feign Death (5384, 30 s) |
| 35 | Kill Shot (53351, 10 s, only on targets at 20 percent health or less); Tranquilizing Shot (19801, 20 Focus) |
| 36 | Scare Beast (1513, 25 Focus, 1.5 s cast) |
| 38 | Explosive Trap (13813, no cost, 30 s); Flare |
| 39 | Trueshot Aura (19506, passive: party and raid +attack power; no cast needed) |
| 40 | Bestial Wrath (19574, 60 s, 10 s, no cost: pet +50 percent damage); Widow Venom (82654); mail armor |
| 43 | Cobra Strikes (53260: Arcane Shot may give the pet critical basic attacks) |
| 45 | tier 3 talent |
| 46 | Ice Trap (13809) |
| 48 | Trap Launcher (77769: traps land at a target location within 40 yd) |
| 50 | The Beast Within (34692: during Bestial Wrath the hunter's shots cost 50 percent less Focus and do 10 percent more damage) |
| 52 | Distracting Shot (20736) |
| 54 | Rapid Fire (3045, 180 s, 15 s: +40 percent ranged haste) |
| 56 | Aspect of the Pack (13159) |
| 58 | Kindred Spirits (56315: +20 maximum Focus for hunter and pet) |
| 60 | tier 4 talent |
| 62 | Call Pet 4 (83244); Invigoration (53253: pet basic attacks may restore 20 Focus) at 63 |
| 66 | Snake Trap (34600) |
| 69 | Exotic Beasts (53270: tame exotic pets) |
| 74 | Master's Call (53271) |
| 75 | tier 5 talent |
| 76 | Misdirection (34477, 30 s, 100 yd) |
| 78 | Deterrence (19263, 5 s of 100 percent deflection) |
| 80 | Mastery: Master of Beasts (76657: pet damage) |
| 81 | Cobra Shot (77767, replaces Steady Shot: gives 14 Focus, lengthens Serpent Sting) |
| 85 | Camouflage (51753) |
| 87 | Stampede (121818, 300 s, 20 s: all pets) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and data tables):

- **No minimum range.** Auto Shot, Arcane Shot, Steady Shot, Multi-Shot, Serpent Sting and the other shots have range 0 to 40 yd
  (SpellRange minimum 0, Kill Shot 45); Method says the same ("no minimum range, ranged attacks usable in melee"). A hunter
  does not need to leave melee range of an enemy.
- Steady Shot and Cobra Shot are 2 s casts that give 14 Focus; Arcane Shot (30) and Multi-Shot (40) are instant spenders.
  Before 81 the filler is Steady Shot only; Cobra Shot does not exist yet.
- Hunter's Mark (+5 percent ranged damage taken by that target from every attacker) is also applied by Arcane Shot, Kill
  Command, Chimera Shot and Explosive Shot (spell texts), so a separate cast is needed only as an opener.
- Aspect of the Hawk and Aspect of the Cheetah share one aspect slot; Cheetah and Pack daze the hunter when struck.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived"
comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/` at
commit `c0291fd`.

How the bots rank what they do (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, ACTION_NORMAL 10, ACTION_HIGH 20,
ACTION_MOVE 30, ACTION_INTERRUPT 40, ACTION_EMERGENCY 90. A trigger edge adds its action with that number; the engine takes
the highest action that is possible and useful. A Beast Mastery hunter gets the `bm` strategy (`BeastMasteryHunterStrategy`,
`Factory/AiFactory.cpp:461-479`) together with `cc`, `dps assist`, `aoe` and `bdps`; every bot outside battlegrounds also gets
`boost`. The non-combat engine gets `bdps`, `dps assist` and `pet` (`:716`) plus the default `nc` strategy
(`GenericHunterNonCombatStrategy.cpp`). The `bm` default actions are listed in `BeastMasteryHunterStrategy.cpp:28-43`: Auto
Shot 6.0, Stampede 5.95, Kill Command 5.9, Kill Shot 5.85, Serpent Sting 5.8, Glaive Toss 5.75, Murder of Crows 5.7, Dire
Beast 5.65, Arcane Shot 5.3, Cobra Shot 5.25, Steady Shot 5.2; Bestial Wrath 19.0. A spell the bot has not learned yet is
simply not cast, so the list degrades by level without extra code.

### Levels 15 to 29

Known: Arcane Shot, Steady Shot, Concussive Shot, Serpent Sting, Kill Command, Aspect of the Hawk, Hunter's Mark, Disengage,
Mend Pet, Counter Shot (22), Multi-Shot (24), Freezing Trap (28); passives Go for the Throat and Beast Cleave. No Bestial Wrath
(40), no Kill Shot (35), no Rapid Fire (54).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, no pet; pet dead | Call Pet 1 (883, 1); Revive Pet (982, 1) | yes, non-combat only (`GenericHunterNonCombatStrategy.cpp:63-66`); no Revive Pet in combat |
| 2 | core | out of combat, no aspect | Aspect of the Hawk (13165, 12) | yes (`HunterBuffStrategies.cpp:17`, BT-054; live check pending) |
| 3 | core | opener on the pull target | Hunter's Mark (1130, 14), first in both guides' lists | yes, `GenericHunterStrategy.cpp:81` (29.5), but on every target that lives half a second (`HunterTriggers.h:150`, `needLifeTime` 0.5): see gap list, rotation 7 |
| 4 | core | Serpent Sting missing and the target lives 8 s or more | Serpent Sting (1978, 10) | yes: "no stings" 17 (`BeastMasteryHunterStrategy.cpp:64`), default 5.8; the 8 s rule is the trigger's default |
| 5 | core | Kill Command ready, pet within 25 yd of the target | Kill Command (34026, 10) | yes: edge 18.5 (`:59`) and default 5.9, gate in `HunterActions.cpp:20` |
| 6 | core | Focus 70 or more | Arcane Shot (3044, 1) as the spender (both guides: only above 70 Focus) | **wrong:** default 5.3 (`:40`) fires at any Focus of 30 or more, so Kill Command (40) and later Chimera-sized spenders starve (inference: Focus arithmetic, to check in a combat log) |
| 7 | core | otherwise | Steady Shot (56641, 3), 14 Focus; Cobra Shot from 81 | yes, default 5.2 (`:42`); Cobra Shot 5.25 is inert before 81 |
| 8 | good | pet below 65 percent health | Mend Pet (136, 16) | yes, `GenericHunterStrategy.cpp:59` (22), low health (40 percent) at 21 (`:86`) |
| 9 | good | the target casts | Counter Shot (147362, 22) | yes, `GenericHunterStrategy.cpp:97` at 41 (the target only; another caster only in PvP, `:99`) |
| 10 | good | a mob is hitting the hunter | Concussive Shot (5116, 8) | yes, "has aggro" and "snare target" edges at 20 (`:83-85`) |
| 11 | good | two or more enemies in 8 yd of the target | Multi-Shot (2643, 24) in place of Arcane Shot (Method and wowtbc: 2 to 3 targets; 4 or more keep Beast Cleave up) | **wrong order:** `light aoe` -> Multi-Shot at 21 (`DpsHunterStrategy.cpp:75`), above Bestial Wrath and every other spender at 2 targets |
| 12 | expert | the leader marked a crowd control target | Freezing Trap (1499, 28) | partly: edge in the `cc` strategy (`GenericHunterStrategy.cpp:130-136`), only for the marked target; the trap lands at the hunter's own feet (range 0 in the data) and nothing launches it: to check live that it ever triggers |

Before a spell is learned (derived): below 10 there is no specialisation, the kit is Arcane Shot, Steady Shot, Concussive Shot and
the pet; Kill Command arrives at 10, so the first dungeon-sized fights already have the full single target list above.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 13 | core | the target is at 20 percent health or less | Kill Shot (53351, 35) before Kill Command (wowtbc: eighth, Kill Command ninth) | **wrong order:** both the edge (18.0, `:61`, against Kill Command's 18.5) and the default (5.85 against 5.9) put Kill Command first |
| 14 | core | Bestial Wrath ready | Bestial Wrath (19574, 40) on the pet, on cooldown (60 s) | yes, default 19.0 (`:28`) with no condition; below the 21.0 Multi-Shot at 2 targets |
| 15 | good | the pet has 5 Frenzy stacks | Focus Fire (82692, 32); may wait a moment for Bestial Wrath (wowtbc) | **missing:** no action named "focus fire" exists (grep, 0 hits) |
| 16 | good | the target has an enrage or a magic effect | Tranquilizing Shot (19801, 35) | yes (`GenericHunterStrategy.cpp:107-110`) |
| 17 | good | two or more enemies attack the hunter | Feign Death (5384, 32) | yes: "medium threat" outside PvP at 35 (`:57`); to check live that the bot stands up and rejoins the fight |
| 18 | good | an enemy is already in melee range of the hunter | Explosive Trap (13813, 38) | yes at 37 (`:55`); guides drop it before the pull and on cooldown in AoE, which no bot does |
| 19 | expert | a beast has to be held | Scare Beast (1513, 36) | only for a marked cc target (`cc` strategy) |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | core | during Bestial Wrath | The Beast Within (50) makes every shot cost half: spend Focus freely (Arcane Shot, Kill Command) | passive; the 70 Focus gate of line 6 should be dropped while the buff is up (derived) |
| 21 | core | a pack of three or more, an elite or a boss | Rapid Fire (3045, 54) on cooldown (180 s), both guides; the bots would time it with Bestial Wrath (derived) | **wrong:** edge 29 (`GenericHunterStrategy.cpp:74`) and boost 16 (`:127`) are `BoostTrigger`s that fire only when the group is out-levelled two to one (`GenericTriggers.cpp:416-424`, inference, to check live) or the target is a player: a normal dungeon pull never gets it |
| 22 | good | a tier 3 talent | Iron Hawk or Spirit Bond (see Talents) | no code for Iron Hawk or Exhilaration |
| 23 | good | an enemy is chasing the hunter | Disengage (781, 14) | **wrong for PvE:** `hunter enemy too close` (`HunterTriggers.cpp:202`) fires when the target is not attacking the hunter and is within melee reach, then Disengage 35, flee 34 (`GenericHunterStrategy.cpp:69-71`); with no minimum range this removes the hunter from a fight it was winning |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 24 | core | tier 4 talent Dire Beast (60) | Dire Beast (120679) with Bestial Wrath (Method: synced; wowtbc: may wait if Wrath is close) | yes, default 5.65 (`:38`), no sync |
| 25 | good | Invigoration (63), Exotic Beasts (69) | passives | no code needed |
| 26 | expert | from 66 | Snake Trap (34600) before the pull and Explosive Trap on cooldown in AoE (Method opener) | missing (no action for Snake Trap; Explosive Trap only on "enemy within melee") |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | A Murder of Crows on cooldown (wowtbc lists it as a tier 75 option; neither BM rotation names it) | default 5.7, with Focus pooling for its 60 cost (`HunterActions.cpp:39`) |
| 76 | Misdirection (not in either guide) | on the main tank when the hunter's threat passes half of the tank's (`GenericHunterStrategy.cpp:89-90`); never before the pull |
| 78 | Deterrence at low health | yes: at 45 percent outside PvP (`:93`) and at 30 percent (`:102`); no effect before 78 |
| 81 | Cobra Shot as the filler, refreshing Serpent Sting | default 5.25 above Steady Shot, present |
| 85, 87 | Camouflage; Stampede on cooldown (first line after the marks) | Stampede default 5.95; no Camouflage |
| 90 | single target (wowtbc order): Hunter's Mark, Serpent Sting, Stampede, Bestial Wrath, Rapid Fire, Rabid, Dire Beast, Kill Shot, Kill Command, Glaive Toss, Focus Fire at 5 Frenzy, Arcane Shot above 70 Focus, Cobra Shot; AoE: Beast Cleave through Multi-Shot, Explosive Trap, Barrage | all but Rapid Fire timing, Rabid (the pet has no specialisation, see "Pet"), Focus Fire, Barrage and the 70 Focus gate |

## Hunter-wide: pet, aspects, traps, positioning

Neither guide covers these (Method: "pet family, Mend Pet, Misdirection, Growl: not mentioned"), so they are derived from the
data and the code. The other two hunter sheets refer here.

### Pet

- **Which pet:** the factory picks a random tameable beast of the bot's level or lower (`Factory/BotFactory.cpp:1000-1020`,
  `Creature.h` `IsTameable`: beast type, any family, exotics only for a hunter who may tame them). The `AiPlayerbot.HunterWolfPet`
  setting in the config (`conf/playerbots.conf.dist:1476`) is read by no code (grep over `src/`, 0 hits). Both guides' lists
  contain Rabid, a Ferocity pet specialisation spell (53401, data: specialisation 74 holds Rabid, Dash, Spiked Collar; Cunning
  79 holds Roar of Sacrifice, Bullheaded, Cornered, Boar's Speed; Tenacity 81 holds Last Stand, Charge, Thunderstomp, Blood
  of the Rhino, Great Stamina).
- **Pet specialisation:** the core sets it only when a client sends the request (`Handlers/PetHandler.cpp:1403`) and a new
  pet starts with none (`Pet.cpp:28`), and the module never calls it (grep `SetSpec` in `src/`, 0 hits). So a bot's pet has none
  of those spells and the guides' Rabid line cannot run (inference from the code, to check live in the pet's spell list).
- **Mend Pet:** the combat engine has it at pet health under 65 percent (22) and under 40 percent (21); out of combat 60
  (`GenericHunterNonCombatStrategy.cpp:65`). Fine for leveling. Revive Pet and Call Pet are non-combat only (`pet` strategy is
  added only to the non-combat engine, `AiFactory.cpp:716`): a pet that dies in a fight stays dead until the fight ends.
- **Growl:** the "toggle pet spell" action (`strategy/actions/GenericActions.cpp:21-70`) switches every autocastable pet spell
  on except the cat's stealth ones, so the pet's Growl is on in every group (derived: players turn Growl off behind a real
  tank because the pet pulls the enemy off the tank; owner question 3).
- **Stance:** Defensive from `AiPlayerbot.DefaultPetStance = 1` (`conf:1481`); the combat engine sends "pet attack" at 40
  (`strategy/generic/CombatStrategy.cpp:18`).
- **Kill Command** needs the pet within 25 yd of the target (spell text; `HunterActions.cpp:20-28` checks it).

### Aspects

- Out of combat the bots cast Aspect of the Hawk (12) through `bdps` (`HunterBuffStrategies.cpp:17`). Nothing re-casts it in
  combat.
- **Aspect of the Cheetah (16) and Aspect of the Pack (56) are never cast:** the strategy that holds them, `bspeed`
  (`HunterBuffStrategies.cpp:22`), is registered but added to no bot (grep over `src/`, `AiFactory.cpp:716` adds only `bdps`).
  So the Pack and Cheetah minor glyphs and Glyph of Pathfinding do nothing, and a hunter follows its leader at walking speed.
  Both aspects daze the hunter when struck (text), so they belong only to travel and following (derived).
- **Trueshot Aura** (39) is a passive in 5.4.8 and needs no cast (BT-146): the cast request is gone from the non-combat
  strategy (`GenericHunterNonCombatStrategy.cpp:48`). The leftover trigger and action classes (`HunterTriggers.h:87-91`,
  `HunterActions.h:166-170`) are dead code.

### Traps

- Explosive Trap (38) is dropped only when an enemy is already in melee reach of the hunter (`GenericHunterStrategy.cpp:55`).
  Both guides open with a trap placed before the pull and use it on cooldown at 2 or more targets; without the Trap Launcher
  (48, 40 yd) the trap lands at the hunter's feet, so a pre-pull trap needs a spot the enemies will run through.
- Freezing Trap (28) and Scare Beast (36) run only for the leader's crowd control mark (`HunterCcStrategy`,
  `GenericHunterStrategy.cpp:130-136`). Ice Trap (46) and Snake Trap (66) have no action. Immolation Trap does not exist for the
  bots: its trainer row is there but its spell has no skill line row (BT-081 removed it).

### Defensives and escapes

- **Feign Death** (32): at two or more attackers outside PvP (`GenericHunterStrategy.cpp:57`).
- **Disengage** (14): see line 23 above; plus the PvP-only edge at 36.5 (`:105`).
- **Deterrence** (78): 45 percent and 30 percent (`:93`, `:102`).
- **Concussive Shot** (8) on a chasing enemy (`:83`).
- **Misdirection** (76): see the brief table.

### Positioning

There is no minimum range in 5.4.8 (data: range 0 to 40 yd for every shot). The bots' "reach spell" prerequisite brings them to
within `AiPlayerbot.SpellDistance` 28.5 yd (`conf:639`) of the target; the hunter does not need to stay out of melee reach. The
only thing that moves a hunter off an enemy in PvE is line 23.

## Cooldowns and defensives

The guides say "on cooldown"; the timing for a dungeon bot is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Bestial Wrath | 40 (60 s) | on cooldown, with Dire Beast | on cooldown at any pack | on cooldown, default 19.0 |
| Rapid Fire | 54 (180 s) | on cooldown | at a pack of three or more, an elite or a boss, with Bestial Wrath | outmatched only (`BoostTrigger`), see line 21 |
| Focus Fire | 32 | at 5 Frenzy stacks | at 5 stacks | missing |
| Kill Command | 10 (6 s) | on cooldown | on cooldown | yes |
| Dire Beast | 60 talent (30 s) | on cooldown, with Bestial Wrath | on cooldown | yes |
| Murder of Crows | 75 talent (120 s) | on cooldown | on cooldown | yes |
| Stampede | 87 (300 s) | on cooldown | on cooldown at a pack | yes (default 5.95) |
| Feign Death, Disengage, Deterrence, Exhilaration | 32, 14, 78, 45 talent | defensives | at low health or when two enemies are on the hunter | first three yes, Exhilaration no |
| Counter Shot | 22 (24 s) | interrupt | any casting enemy | the target only |

## Healer triage and mana (healers only)

Not applicable. Focus is the resource (the bots read the hunter's power type correctly since BT-101).

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Cobra Shot (81), Misdirection (76), Deterrence (78), Stampede (87), Camouflage (85), Master of Beasts
  (80) and the tier 5 and 6 talents are not available in the 15 to 60 band; the lists above start without them.
- Cobra Shot is the filler of the guides; before 81 the filler is Steady Shot (and wowtbc's Marksmanship AoE line "Cobra Shot
  for Focus" cannot be right: Cobra Shot is a Beast Mastery and Survival spell only).
- Rabid is a pet specialisation spell; a bot's pet has no specialisation (above).
- Kill Shot needs 35 and Rapid Fire 54; both guides' "Rapid Fire on cooldown" does not apply below 54.
- Method's Survival page mentions Beast Cleave; in the data it belongs to Beast Mastery only (specialisation 253, level 24).
- Hit and Expertise to 7.5 percent is a level 90 number.
- Trueshot Aura is a passive for every specialisation from 39; Method lists it as a change from Cataclysm.
- No minimum range (confirmed in the data), so a rule that backs a hunter out of melee reach has no 5.4.8 basis.
- The module's ranged weapon speed preference and the "armor penetration" weight are Wrath rules.

## Gap list (ordered by value for levels 15 to 60)

Config (can go at once):

1. Talents: `PremadeSpecLink.3.0` (`conf/playerbots.conf.dist:2712`) to `201100` once the Iron Hawk action exists, else
   `202100`. Today the level 45 pick is a talent with no code, and the level 15 pick differs from Method.
2. Glyphs: `PremadeSpecGlyph.3.0` (`:2713`) to `42909,43338,42900,43355,45732,42903`. Today a level 50 to 77 Beast Mastery
   bot carries Deterrence (nothing before 78), Pathfinding and the Cheetah and Pack glyphs (no code casts the aspects).

Rotation (class fixes, wait for the owner's in-game test):

1. **Arcane Shot Focus gate** (core, biggest for this specialisation): both guides spend Arcane Shot only above 70 Focus;
   `BeastMasteryHunterStrategy.cpp:40` casts it at any Focus of 30, so Kill Command (40) and the 45 to 60 Focus spenders of the
   other specialisations starve. Fix: an `isUseful` on `CastArcaneShotAction` (`HunterActions.h`) that wants 70 Focus, or
   Focus 30 while The Beast Within is up. BT-056 closed this as "rotation priority"; the priority argument holds only when the
   Focus is there, which is the point of the 70 rule (inference, check with a combat log first).
2. **Rapid Fire timing** (good): add a rule beside the `BoostTrigger` edges (`GenericHunterStrategy.cpp:74`, `:127`) for dungeon
   packs of three or more and elites, from level 54.
3. **Multi-Shot order** (good): `DpsAoeHunterStrategy` (`DpsHunterStrategy.cpp:75`) at 21.0 and two enemies should become "in place
   of Arcane Shot" (same Focus gate), spam only from four (Method and wowtbc).
4. **Focus Fire** (good, Beast Mastery only): new action and an edge on the pet's Frenzy aura (19615) at 5 stacks, from level 32.
5. **PvE Disengage and flee** (good): drop or narrow `HunterEnemyTooCloseTrigger` (`HunterTriggers.cpp:202`) for PvE: no
   minimum range exists, and the edge fires exactly when the tank holds the target. Owner decision (question 4); BT-056
   closed the Disengage question for the same edge as "design".
6. **Pet** (good): Growl off in groups with a real tank (`GenericActions.cpp:21`, exclude Growl from the autocast list); pet
   family and specialisation (`BotFactory.cpp:1000`, `Pet::SetSpec`) so that Rabid exists: owner question 2.
7. **Hunter's Mark on trash** (good): the trigger (`HunterTriggers.h:150`, `needLifeTime` 0.5) spends a global cooldown on every
   mob although Arcane Shot and Kill Command apply the mark; raise `needLifeTime` or restrict to elites and the first target of
   a pack.
8. **Kill Shot before Kill Command** (good): edge 18.0 below Kill Command's 18.5 (`:59-62`) and default 5.85 below 5.9 (`:33-34`).
9. **Aspect of the Iron Hawk** action and trigger (`HunterBuffStrategies.cpp`) if the guide talent is chosen.
10. **Aspect of the Cheetah and Pack** for following and travel (`bspeed` is never added: `AiFactory.cpp:716`); the aspects must
    drop in combat or when the hunter is struck (the glyph turns the daze into a cooldown).
11. **Revive Pet and Call Pet in combat** (good): move or copy the `pet` edges into the combat engine (they cost 35 Focus and a
    4 s cast).
12. **Exhilaration** (expert, only if the talent stays): an action beside Deterrence at low health.
13. Traps: Explosive Trap pre-pull and in AoE (needs the Trap Launcher toggle), Snake Trap (66), Ice Trap; expert tier.
14. Stat weights (`Mgr/Item/StatsWeightCalculator.cpp:373-384`): Critical above Mastery above Haste for Beast Mastery and
    Survival (Method), Expertise weight, remove the dead Careful Aim and Hunter vs. Wild lines (`:702-707`).
15. Skill tiers: no hunter strategy has tier gating (`BotSkill` is used only in the tank code). Lines marked good and expert
    above are the ones a casual bot should drop.
16. To check live: BT-053 to BT-055 (Auto Shot order, Hawk, cc strategy) are deployed with their live check pending; lines 2 and
    7 depend on them. Also: that Counter Shot (22) is known by every hunter bot; that a Marksmanship or Survival bot can cast Kill
    Command; what a bot pet's spell list holds.

### Owner questions

1. **Talents.** Guide choice `201100` (Crouching Tiger, Aspect of the Iron Hawk; needs one small action) or bot-friendly
   `202100` (Spirit Bond, no code). Tier 15: Crouching Tiger (guide) or Posthaste (today)?
2. **Pet family.** Today a random beast with no specialisation. Choose: random (as now), always Ferocity (the guides' Rabid)
   with the pet specialisation set by code, or Tenacity for a hunter leveling alone. The unused `AiPlayerbot.HunterWolfPet`
   setting would be the natural switch.
3. **Growl.** Off in groups that have a tank (players' habit, not in the guides), or on as today.
4. **Disengage and flee against PvE enemies.** Remove (no minimum range in 5.4.8) or keep for solo leveling only.
5. **Aspect of the Pack or Cheetah while following the leader.** Add (a hunter then keeps up), or leave as is.
6. **Rapid Fire.** On cooldown at packs of three or more and elites (the guides), or only when outmatched (today).

## PvP play (player guides, 5.x, read 2026-11-19)

- **Sources:** one 5.4 PvP guide and one 5.3 battleground clip by a player who says he plays Marksmanship in arena and Beast Mastery only in battlegrounds, plus one MoP Classic PvE guide used for rotation basics only (flagged "PvE"). No coach course. Facts only, nothing copied; data checked against this server's 5.4.8 tables.
- **Coverage:** thin. One arena-adjacent video, almost nothing on defensives beyond glyphs and Roar of Sacrifice, no arena game. Most of the arena plan below is adapted from the Marksmanship and Survival plans and checked against this specialisation's spells; those lines say "adapted".

**What the guides say.** Beast Mastery is viable in battlegrounds and scales better with poor gear because the pet carries the damage; a geared player moves to Marksmanship. Burst is claimed equal to Marksmanship. Burst order: Stampede, Rapid Fire and Bestial Wrath together, Arcane Shot spam, Kill Command and the pet stun on cooldown; press Focus Fire only when the hunter, not the pet, deals the damage (it eats the Frenzy stacks). Rotation basics (PvE): Hunter's Mark, Serpent Sting, Kill Command with 40 focus pooled, Kill Shot under 20 percent, Focus Fire at 5 Frenzy stacks, Arcane Shot to dump focus, Cobra Shot filler, Glaive Toss and Crows on cooldown; Stampede, Dire Beast and Bestial Wrath not at the same time.

**Arena plan (adapted from the Marksmanship and Survival plans).** Chain control: Scatter Shot then Freezing Trap on the healer (they do not share diminishing returns; trap off a partner stun), Wyvern Sting as an instant extra control and a cast stop; Deterrence when the healer is crowd controlled or early into a one-shot comp; Disengage after the enemy's Charge, then Concussive Shot; Master's Call and Roar of Sacrifice for the pet; Flare against stealth at the start; Widow Venom on the healed target; Stampede right before Rapid Fire only for an all-in, else hold it for the next crowd control chain.

**Talents and glyphs, guides against the set** (`PremadeSpecLinkPvp.3.0 = 002000`, glyphs `42917,45732,42908,43338,43355,42909`):

| Tier | Guides | Set | Ladder (1 player) |
|---|---|---|---|
| 15 | Posthaste (kiting) or Narrow Escape (battlegrounds) | Posthaste | Posthaste |
| 30 | Wyvern Sting for arena; Intimidation behind poles (BM only); Binding Shot in battlegrounds | Binding Shot | Wyvern Sting |
| 45 | Spirit Bond (no healer) or Aspect of the Iron Hawk (with a healer) | Spirit Bond | Spirit Bond |
| 60 | Thrill of the Hunt for BM (Arcane Shot is the spender); Fervor not needed | Fervor | Thrill of the Hunt |
| 75 | Lynx Rush or Blink Strikes; Crows "too much focus" for BM (the Marksmanship and Survival plans want Crows) | A Murder of Crows | Crows |
| 90 | Glaive Toss | Glaive Toss | Powershot |

Glyph the guide calls crucial for arena: Solace (Freezing Trap and Scatter Shot also remove damage over time). The rest is situational (Animal Bond with a healer, Explosive Trap as a knockback, Camouflage). Pets: ferocity for Heart of the Phoenix, cunning for Roar of Sacrifice. Stat order for PvP: GAP (the PvE guide: agility, hit and expertise caps, crit, haste, mastery).

**Differences from the guides (5.4.8).** Bestial Wrath breaks loss of control "of your pet" only, and The Beast Within gives damage and cheaper shots with no immunity effect in its aura list, so the guide's "BM enrage is a free trinket" is not in the data (to check live). Stampede pets deal 25 percent damage in arenas and battlegrounds (effect 130201), so Stampede is control and pressure more than damage. Glyph of Mirrored Blades only reflects a spell while in Deterrence (the guide describes a general reflect). Master's Call (level 74) is worded as the pet removing effects from "itself and its target"; whether the hunter can be that target is not in the text. Cobra Shot is learned at 81 (Steady Shot before). Blink Strikes is a tier 75 talent (name with an s). Level gates: Kill Command 10, Frenzy 30, Focus Fire 32, Bestial Wrath 40, The Beast Within 50, Rapid Fire 54, Master of Beasts 80, Stampede 87.

**Bots today (read from `src/strategy/Classes/hunter/`, ordered by value).**
1. Wyvern Sting has an action but no trigger or edge (`HunterActions.h:112`); no Scatter Shot anywhere; Freezing Trap only on the dungeon mark (`GenericHunterStrategy.cpp:149`).
2. Stampede is the top default action (`BeastMasteryHunterStrategy.cpp:32`), so it goes out as soon as it is ready, not behind a control chain or with Rapid Fire; Rapid Fire is a plain 29.0 edge.
3. No Master's Call, Roar of Sacrifice, Widow Venom or Flare.
4. Disengage in PvP: the PvP trigger returns false for any bot in a group (`HunterTriggers.cpp:158`), an arena team is a group (inference, to check live), and the generic "too close" Disengage is for outside PvP instances (`GenericHunterStrategy.cpp:82-83`).
5. Deterrence at 30 percent health (`:107,116`, `HunterTriggers.cpp:151`), not tied to the healer being crowd controlled; Counter Shot on casters exists (`:111-114`).
6. What fits the guides: Kill Command, Kill Shot, Serpent Sting, Glaive Toss, Crows and Dire Beast defaults (`BeastMasteryHunterStrategy.cpp:28-40`), Focus Fire at 5 Frenzy stacks (`:53`), Bestial Wrath on cooldown (`:28`), Binding Shot while the target is 15 to 30 yd away (`:59`), Intimidation as a buff-style edge (`:56`).

**Research update (public patch notes 5.1 to 5.4.7, a 2013 changelog thread and spell data, 2026-11-19; lines marked derived come from game knowledge and the 5.4.8 data).** Corrections: Deterrence has two charges and a 3 minute cooldown in 5.4, Disengage is 20 s, Counter Shot is a baseline kick for every hunter from level 22 (Silencing Shot is the Marksmanship version), Wyvern Sting reaches 40 yd, Hunter's Mark lasts 20 s in PvP combat, Dismiss Pet is blocked in arenas (5.4.2), Beast Cleave is 75 percent of the damage on nearby targets after a Multi-Shot (5.3), and Bestial Wrath breaking pet crowd control is the 5.1 change (the hunter is not in its text). Data: Scatter Shot 30 s, 20 yd, 4 s; Master's Call 45 s, 40 yd. Derived: Beast Mastery's arena niche is Beast Cleave with a stacking team; keep the pet on passive until the go so it does not wake a trap, and release it with the first control (Blink Strikes puts it on the target); Scatter Shot then Freezing Trap chain (disorient and incapacitate do not share), Freezing Trap then Wyvern Sting on one target halves the second (both incapacitates); use the first Deterrence charge as a tempo tool and the second as the last immunity. Extra gap lines: Deterrence fires at 30 percent health only (`HunterTriggers.cpp:151-154`); no Multi-Shot edge for Beast Cleave; the pet is never held on passive.

**Proposals.** Config (no code): tier 30 Wyvern Sting only after a Wyvern Sting edge exists; tier 60 Thrill of the Hunt has no rule (Fervor has code), so it stays Fervor until a rule exists; keep Glaive Toss; Glyph of Solace is already in the set. Rotation (waits for the owner's in-game test): Wyvern Sting and Scatter Shot plus trap chain, Master's Call and Roar of Sacrifice for the pet, Stampede with Rapid Fire in a control window, a Flare at the start against stealth.

**Open questions.** (1) Whether Beast Mastery should be an arena bot at all (the source prefers Marksmanship). (2) The Beast Within claim (live check). (3) Whether the Disengage trigger works in an arena group.

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 1 player of this specialisation (rating 2507 to 2507). 2v2: no player of this specialisation. Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** none, only 1 player in all (noted, not changed)
- **BT-324 result:** open (1 player)
- **Set now:** `PremadeSpecLinkPvp.3.0 = 002000`, `PremadeSpecGlyphPvp.3.0 = 42917,45732,42908,43338,43355,42909`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Posthaste 100% / - / 1 of 1 | Posthaste | Posthaste | matches |
| 2 (30) | Wyvern Sting 100% / - / 1 of 1 | Binding Shot | Binding Shot | differs, not changed |
| 3 (45) | Spirit Bond 100% / - / 1 of 1 | Spirit Bond | Spirit Bond | matches |
| 4 (60) | Thrill of the Hunt 100% / - / 1 of 1 | Fervor | Fervor | differs, not changed |
| 5 (75) | A Murder of Crows 100% / - / 1 of 1 | A Murder of Crows | A Murder of Crows | matches |
| 6 (90) | Powershot 100% / - / 1 of 1 | Glaive Toss | Glaive Toss | differs, not changed |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Explosive Trap (42908) | 100% | - | 1 of 1 | yes | yes |
| major | Aimed Shot (42914) | 100% | - | 1 of 1 | no | no |
| major | Solace (42917) | 100% | - | 1 of 1 | yes | yes |
| major | Animal Bond (42909) | 0% | - | 0 of 1 | yes | yes |
| minor | Tame Beast (42912) | 100% | - | 1 of 1 | no | no |
| minor | Lesser Proportion (43350) | 100% | - | 1 of 1 | no | no |
| minor | Revive Pet (43338) | 0% | - | 0 of 1 | yes | yes |
| minor | Aspect of the Pack (43355) | 0% | - | 0 of 1 | yes | yes |
| minor | Aspect of the Cheetah (45732) | 0% | - | 0 of 1 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **Open (BT-451):** one player in 3v3, none in 2v2: tiers 15, 45 and 75 match; tier 30 (Wyvern Sting), tier 60 (Thrill of the Hunt) and tier 90 (Powershot) differ from the set (Binding Shot, Fervor, Glaive Toss). The survival set (5 players) moved to Wyvern Sting at tier 30.
