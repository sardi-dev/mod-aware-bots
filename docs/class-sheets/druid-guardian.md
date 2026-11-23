# Druid: Guardian (tank)

- **Status:** draft, checked against 5.4.8 data and against the bots' code by reading (2026-11-10). Rotation gap items 1
  to 4 are implemented on `feat/class-sheet-bear-disc-protect` (BT-131, compile pending, owner tests them in his next
  dungeon run); the rest is open and nothing is verified live. The "Bots today" columns below describe the code before
  that change.
- **Sources:**
  - Method, "Guardian Druid Class Guide, MoP Classic" (Phase 1, level 90 raid and dungeon, 22 July 2025), read
    2026-11-10.
  - wowtbc.gg, "Guardian Druid" in the MoP class guides (MoP Classic, no patch line on the page), read 2026-11-10.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent,
    GlyphProperties, SpellCooldowns, SpellCategories, SpellCategory, SpellPower, SpellRange, SpellMisc, SpellEffect,
    SpellDuration, SpellCastTimes; `data/db2/Item-sparse.db2` for glyph item ids) through the data reader and short
    read-only scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch), Wowhead (script-built page); one guessed third-party URL
    was 404. Two guides were read, not three.
- **Levels covered:** 15 to 60 in detail (5-player dungeons, a human leader who marks targets), 61 to 90 briefly.

Which source was richer: **talents** Method (it gives picks and the dungeon swap; wowtbc lists every option without a
pick); **glyphs** equal (same three majors); **stat order** Method (wowtbc has none); **single target priority** both
thin and they disagree on the order (see below); wowtbc is richer because it names Savage Defense and Frenzied
Regeneration as steps and gives an opener; Method names Berserk and the maintain lines; **area priority** Method
(wowtbc has none); **defensives** equal (same three, wowtbc gives an order); **interrupts, taunts, the pull, levels 15 to
60, what to do before a spell is learned, thresholds** neither guide says anything, so those lines are derived from
the 5.4.8 data and marked "derived".

Guide order disagreement and the pick: Method lists Faerie Fire, Berserk, Thrash, Mangle, Maul on Tooth and Claw,
Lacerate; wowtbc lists Berserk, Faerie Fire, Savage Defense and Frenzied Regeneration, Maul, Mangle, Thrash, Lacerate.
Both put the debuffs before Mangle because at level 90 they are refresh lines, not fillers. The data shows why that does
not mean "cast Faerie Fire every global": Faerie Fire and Thrash have no cooldown and only refresh a 30 s debuff, while
Mangle is the rage source (5 rage, 6 s cooldown). The lists below read the guides as "keep the two debuffs up, then
Mangle on cooldown, then fill" (derived).

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.11.2 = 200010`
(`conf/playerbots.conf.dist:2918`; one digit per tier, the column counted from 0). For a character below 75 only the
first four digits are in effect.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (200010) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Wild Charge or Feline Swiftness (Method: by movement needs; wowtbc opener uses Wild Charge) | Displacer Beast (wowtbc lists only) | 102401 / 131768 | Wild Charge, same |
| 2 (30) | Ysera's Gift (Method: the only locked talent) | Renewal, Cenarion Ward (wowtbc lists only) | 145108 | Ysera's Gift, same |
| 3 (45) | Faerie Swarm, Typhoon or Mass Entanglement (Method: by encounter) | | 106707 / 132469 / 102359 | Faerie Swarm, one of Method's three |
| 4 (60) | Soul of the Forest (Method preferred) | Incarnation: Son of Ursoc (Method: for bursty dungeon pulls), Force of Nature | 114107 / 106731 / 106737 | Soul of the Forest, same |
| 5 (75) | Mighty Bash (Method default) | Disorienting Roar, Ursol's Vortex (Method: situational) | 5211 / 99 / 102793 | **Ursol's Vortex (102793), differs (a Method alternative)** |
| 6 (90) | Dream of Cenarius (Method: best overall) | Nature's Vigil (Method: dungeon), Heart of the Wild (Method: better in raids) | 108373 / 124974 / 108288 | **Heart of the Wild (108288), differs (Method's raid pick); no bot line casts it** |

Notes on the picks (data, not guide):

- Soul of the Forest (Guardian text): Mangle (Bear) generates more rage and hits harder. The Mangle text shows 5 rage
  and 7.5 rage when Soul of the Forest and Savage Defense are both known (spell 33917 and 33878), a 50 percent rise on
  the main rage source, which matters because Savage Defense costs 60 rage (derived). Keep it.
- Faerie Swarm replaces Faerie Fire on the action bar (override aura, spell text "replaces Faerie Fire") and adds a
  50 percent snare. The bot casts spell 770 by name, so the snare is lost; the pick is harmless but idle. Whether
  casting 770 after the talent still applies Weakened Armor is **to check live**. Typhoon (knockback) scatters a pack
  from the tank and is a bad dungeon pick (derived).
- Incarnation: Son of Ursoc (3 min, 30 s): melee abilities and Growl drop to a 1.5 s cooldown. Method's dungeon pick; no
  bot line exists (the module's only "incarnation" action is the restoration tree form, `DruidShapeshiftActions.h:44`).
  Force of Nature (Guardian treant "will use Taunt", spell text) has a line in the boost strategy
  (`GenericDruidStrategy.cpp:213`).
- Nature's Vigil (90) has a line for the Guardian (`GenericDruidStrategy.cpp:203-204`, boost). Heart of the Wild,
  Ursol's Vortex and Mighty Bash have none. The tier 5 talent is therefore never cast; Mighty Bash (50 s, single stun,
  all forms) would be the guide default once a line exists.
- Wild Charge (102401) is the talent; the spell text shows the forms as separate spells (16979 "charge to an enemy,
  immobilizing them" is the bear one). Whether the bot's spell book resolves the bear version is **to check live**
  (`DruidBearActions.h:12-22`, the comment there says the same).

Proposed link by the guide (dungeon, owner decision on tier 5): `200022` (Wild Charge / Ysera's Gift / Faerie Swarm /
Soul of the Forest / Mighty Bash / Nature's Vigil). The tier 6 change is the one with code behind it.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and
75 (config comment; the unlock levels are core-side, not in the data files). Config order is major, minor, major, minor,
minor, major (`conf/playerbots.conf.dist:2919`).

| Type | Glyph | Item id | Why (one clause) | Bots today (`40897,43332,40909,43316,44922,45604`) |
|------|-------|--------:|------------------|------------------------|
| Major | Maul | 40897 | Maul hits one additional target (spell text) | yes (slot 1, level 25) |
| Major | Rebirth | 40909 | the resurrected player returns with more health; Rebirth is learned at 56 | yes (slot 3, level 50; idle until 56) |
| Major | Stampeding Roar | 45604 | larger radius; the spell is learned at 84 | yes (slot 6, level 75; idle until 84) |
| Major, optional | Survival Instincts, Might of Ursoc | 45601, 45603 | shorter cooldown but shorter duration, or more health but longer cooldown (Method: raid options) | no |
| Major, derived | Frenzied Regeneration | 40896 | Frenzied Regeneration always costs a fixed rage and gives a healing-received buff instead of converting rage (a different spell; not recommended while the bot's rule is rage based) | no |
| Major, derived | Barkskin | 45623 | 25 percent less chance to be critically hit during Barkskin (learned at 44) | no |
| Major, derived | Faerie Fire, Fae Silence | 67485, 67484 | +5 yards range; or a 3 s silence from Bear Form with a 15 s cooldown (an interrupt tool; collides with Faerie Fire as filler) | no |
| Minor | Grace, Aquatic Form, Stars | 43332, 43316, 44922 | no tanking effect (Method: take whichever; wowtbc names Grace) | yes (slots 2, 4, 5), harmless |

Disagreement: none (both name Maul, Rebirth, Stampeding Roar; Method adds the two optional raid glyphs). The premade
matches. For the 15 to 60 band two of the three majors are idle at the levels they unlock (Rebirth until 56, Stampeding
Roar until 84); a glyph that works earlier would be Barkskin (44) or Survival Instincts (56) (derived, owner decision, low
value).

## Stat order

Method: Hit to 7.5 percent, Expertise to about 15 percent, Agility, Stamina, Critical Strike (for rage), Haste, Mastery
(least; wowtbc gives none). Those caps are level 90 numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason): **leather** with a high armor value first (Bear
Form multiplies the armor of cloth and leather items, 120 percent, or 330 percent with Thick Hide from level 14, spell
text of 5487), then Agility and Stamina (Agility feeds attack power and dodge in Bear Form, Stamina is the survival
stat), then Critical Strike (more Mangle rage per Method), Haste; **Mastery is worth nothing below level 80** (Mastery:
Nature's Guardian, an armor bonus, is learned at 80, so earlier Mastery rating has no effect); Hit and Expertise only
near level 90. A druid has no shield and no mandatory weapon type; any usable weapon of a higher level is an upgrade.

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts; the data holds several old rows of the same name. Learn levels come from
SpellLevels (class line and the specialisation's own list). Bear abilities need Bear Form (learned at 8).

| Level | Spell (id) |
|------:|------------|
| 1 | Wrath (5176, 2 s cast, 40 yd) |
| 3 | Moonfire (8921, instant, 40 yd) |
| 4 | Rejuvenation (774) |
| 6 | Mangle (33917; bear: 280 percent weapon damage, 5 rage, 6 s cooldown), Cat Form (768) |
| 8 | Bear Form (5487; sets rage to 10), Growl (6795, 30 yd, 8 s, off the global cooldown), Maul (6807, 30 rage, 110 percent weapon damage) |
| 10 | Savage Defense (62606, 60 rage, 3 charges, one charge back per 9 s, +45 percent dodge for 6 s), Vengeance (84840, passive), Entangling Roots (339); the specialisation is chosen here |
| 14 | Thick Hide (16931, passive: Bear Form armor bonus 330 percent, less damage taken, Barkskin cooldown 30 s shorter) |
| 18 | Aquatic Form (1066), Bear Hug (102795) |
| 22 | Swipe (106785, free, no cooldown, hits all nearby), Remove Corruption (2782) |
| 26 | Healing Touch (5185, 2.5 s cast) |
| 28 | Thrash (bear effect 77758, cast as 106832: free, no cooldown, 8 yd, bleed, Weakened Blows 10 percent less physical damage dealt for 30 s, chance to reset Mangle), Faerie Fire (770, 35 yd, no cooldown, three Weakened Armor stacks of 4 percent each for 30 s) |
| 32 | Tooth and Claw (135288, passive: auto attacks can empower the next Maul) |
| 38 | Lacerate (33745, free, bleed, stacks to 3, chance to reset Mangle) |
| 40 | Infected Wounds (48484, passive: Mangle and Maul snare) |
| 44 | Barkskin (22812, 12 s, 20 percent less damage taken, 60 s cooldown, 30 s with Thick Hide, usable in all forms) |
| 45 | tier 3 talent |
| 46 | Leader of the Pack (17007, passive) |
| 48 | Berserk (106952, 180 s, 10 s in Bear Form: Mangle has no cooldown and hits up to 3 targets) |
| 50 | Leather Specialization (86096, passive) |
| 52 | Nature's Grasp (16689, 60 s, enemies that strike the druid are rooted) |
| 54 | Innervate (29166) |
| 56 | Survival Instincts (61336, 180 s, 12 s, 50 percent less damage taken), Rebirth (20484, 2 s cast, in combat) |
| 60 | tier 4 talent |
| 62 | Mark of the Wild (1126) |
| 64 | Skull Bash (106839, 15 s, 13 yd charge, interrupts and locks the school) |
| 66 | Hibernate (2637) |
| 68 | Frenzied Regeneration (22842, converts up to 60 rage into health) |
| 72 | Might of Ursoc (106922, 180 s, 20 s, +30 percent current and maximum health, forces Bear Form) |
| 75 | tier 5 talent |
| 76 | Enrage (5229, 60 s, gives rage) |
| 78 | Cyclone (33786) |
| 80 | Mastery: Nature's Guardian (77494) |
| 84 | Stampeding Roar (106898) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and tables of the data): Mangle, Thrash, Lacerate and Faerie Fire (cast in Bear
Form) can each reset Mangle's cooldown (the "Mangle!" proc, 25 percent); Maul "deals additional damage against bleeding
targets"; Thrash and Lacerate are bleeds; Tooth and Claw empowers a Maul that also reduces the victim's next auto attack;
Bear Form text: "significantly increases threat generation", armor and Stamina; Savage Defense has three charges so a
second use does not wait for the first; Frenzied Regeneration spends up to 60 rage and heals for what it spent; there is
**no area taunt** (Challenging Roar has no row in the 5.4.8 data) and Growl is single target.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` (module repository). Relevance numbers: default actions sit at 5.0 plus a fraction, high 20,
interrupt 40, emergency 90 (`strategy/Strategy.h:37-51`).

### The pull (all bands; a human leader marks, the tank pulls marks only, the group waits)

Setup verified by reading: the Guardian carries `bear`, `tank assist`, `pull`, `pull back`, `feral charge` in combat
(`Factory/AiFactory.cpp:457`) and `tank assist`, `pull` out of combat (`AiFactory.cpp:742`); any tank also gets `mark
pull` (`AiFactory.cpp:886`). The trigger fires when a mark exists, is alive, not in combat, within 40 yd and every member
is alive, out of combat, above 60 percent health and (mana users) 50 percent mana (`strategy/triggers/MarkTriggers.cpp:49-84`,
`:32-45`); then `pull rti target` (`strategy/generic/MarkPullStrategy.cpp:12`).

**Does the bear have the warrior's problem (a tank without its ranged pull spell refusing or stalling every pull)? Not
the refusal, and the stall only in two narrow cases; but the pre-pull step is broken.** Verified by reading:

- The druid is exempt from the "shoot needs a ranged weapon" refusal (`strategy/generic/PullStrategy.cpp:150-152`), so a
  pull request is accepted at any level.
- Pull action choice (`strategy/Classes/druid/DruidPullStrategy.cpp:26-43`): the base action is "faerie fire"
  (`DruidPullStrategy.h:16`); the test for "faerie fire (feral)" at `:30` can never be true (that name is not a 5.4.8
  spell, comment at `DruidActions.h:21`), so the choice is Faerie Fire when it can be cast, else Growl. Faerie Fire is
  learned at 28, so levels 8 to 27 pull with Growl and 28 up with Faerie Fire (35 yd, any form). `CanCastSpell` accepts a
  spell that fails only for the wrong form or range (`AI/PlayerbotAI.cpp:3856-3867`), so Growl is chosen even from caster form.
- **Growl needs Bear Form and the pre-action cannot shift.** The pre-action for a Growl pull is "dire bear form"
  (`DruidPullStrategy.cpp:45-50`, `DruidPullStrategy.h:16`). There is no 5.4.8 spell of that name (no levelled row), so
  `CastDireBearFormAction::isPossible` is false (`CanCastSpell` with no spell id, `AI/PlayerbotAI.cpp:3782`), the pull
  start action returns false (`strategy/actions/PullActions.cpp:138-140`) and the shift never happens. The alternative
  "bear form" that the combat strategy uses (`DruidShapeshiftActions.cpp:20-24`) is not reached by the pre-action call
  (`PlayerbotAI::DoSpecificAction` returns the impossible result, `AI/PlayerbotAI.cpp:1690-1697`). The pull action itself
  still runs as the default action (`PullStrategy.cpp:235`), so a Growl pull works only when the bear is already in Bear
  Form (a leftover from the last fight) and stalls to the 15 s limit (`PullStrategy.h:41`) from caster form (the Growl
  cast fails for the form). Stall length and how often the bear is in caster form at levels 10 to 27 are **to check live**.
- From level 28 the pull is Faerie Fire and has no pre-action (`DruidPullStrategy.cpp:47-48`), so the bear pulls in
  whatever form it is in. Nothing shifts it into Bear Form out of combat (no bear form edge in a non-combat strategy;
  the only edge is in the combat strategy, `BearTankDruidStrategy.cpp:176-177`). While a pull is pending or started the
  pull multiplier blocks every other action (`PullStrategy.cpp:183-198`), so the shift to Bear Form comes only after the
  pull ends (mob in reach and the bear back at the pull position, `PullTriggers.cpp:30-60`), that is with the mob already
  on the tank in caster form for about one global (inference from the order of the checks; **to check live**).

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| P1 | core | the group is ready and a mark is up (derived: Bear Form has the armor and the threat bonus; Growl needs it) | shift to Bear Form (5487, 8) before the pull | **gap: pre-action "dire bear form" never resolves (`DruidPullStrategy.cpp:45-50`); no non-combat shift** |
| P2 | core | levels 8 to 27, mark within 30 yd, in Bear Form | Growl (6795, 8) | yes as the choice (`DruidPullStrategy.cpp:39-40`), but only works from Bear Form (see above) |
| P3 | core | level 28 and up, mark within 35 yd | Faerie Fire (770, 28) | yes (`DruidPullStrategy.cpp:26-43`); no form shift first (P1) |
| P4 | good | Growl on cooldown (8 s) and Faerie Fire not known (levels 8 to 27) (derived: Moonfire is instant, 40 yd and learned at 3, but a druid in Bear Form cannot cast it; so the fallback is to wait one cooldown, not to cast) | wait for Growl, or Moonfire (8921, 3) from caster form | **gap: the choice falls back to "faerie fire" (unknown) when Growl is on cooldown and the pull stalls until the 15 s limit** (to check live) |
| P5 | good | the pull target is hit; mobs come to the tank | stay at the pull position | yes: `pull back` (`PullStrategy.cpp:262-268`) |
| P6 | good | Wild Charge talent (15): the mark is beyond reach after the group moved | Wild Charge, bear version (16979, 8 to 25 yd) | in combat only (`BearTankDruidStrategy.cpp:172-173`, `FeralDruidStrategy.cpp:132`); not a pull action; the bear spell may not resolve (to check live) |

### Levels 15 to 29

The Guardian has Mangle, Maul, Growl, Savage Defense, Swipe (22) and Thick Hide (14). No Thrash (28), no Faerie Fire (28),
no Lacerate (38), no defensive cooldown (Barkskin 44, Survival Instincts 56), no interrupt (Skull Bash 64).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | always in a fight (derived: Growl, Mangle and Savage Defense need it) | Bear Form (5487, 8) | yes in combat: trigger "bear form" shifts at relevance 28 through "dire bear form" whose alternative "bear form" casts (`BearTankDruidStrategy.cpp:176-177`, `DruidShapeshiftActions.cpp:20-24`, `strategy/Engine.cpp:192-247`); costs one global at the start |
| 2 | core | an enemy attacks someone else (healer, DPS) | Growl (6795, 8), 30 yd | yes: `lose aggro` on the current target (`BearTankDruidStrategy.cpp:197-199`, `GenericTriggers.cpp:155-161`, never on a player; `DruidBearActions.h:26-32`); the tank target picks mobs without aggro first (`strategy/value/TankTargetValue.cpp:74-116`) |
| 3 | core | Mangle is ready (6 s) | Mangle (33917, 6) | yes (`BearTankDruidStrategy.cpp:185-186` at 17.5, default `:163`), **from 28 outranked by Faerie Fire (gap 1)** |
| 4 | core | rage 60 or more and the previous Savage Defense is gone or less than 2 s left (3 charges, so it can be recast at once) | Savage Defense (62606, 10), proactive against physical | **late: fires only at rage 80 (`DruidTriggers.h:489-494`, `BearTankDruidStrategy.cpp:222-223`) and Maul spends rage from 45 (`DruidBearActions.cpp:10-13`), so the threshold is rarely reached** |
| 5 | core | two or more enemies within 8 yd, from 22 | Swipe (106785, 22) | yes (`BearTankDruidStrategy.cpp:204-205` at 25 for two, `:201-203` at 26 for three); no cost, no cooldown |
| 6 | good | rage above what Savage Defense needs (derived: 60 reserved, Maul costs 30, so Maul only from 90) or an empowered Maul | Maul (6807, 8) | **wrong threshold: from 45 rage (`DruidBearActions.cpp:10-13`), default priority 5.2 (`BearTankDruidStrategy.cpp:165`)** |
| 7 | good | nothing above is ready | auto attack | yes (`BearTankDruidStrategy.cpp:166`) |

What a tank does where a key spell is missing (derived): levels 10 to 21 have no Swipe, so Mangle and Maul are the whole
attack kit and threat on more than one mob comes from Growl and auto attacks; levels 15 to 43 have no active defensive
cooldown (Barkskin is 44), so survival rests on Bear Form armor, Savage Defense dodges and the healer; no interrupt until
64. The bots tolerate every unlearned spell because an unknown spell is simply not cast (inference from the call sites).

### Levels 30 to 44

As above, plus (Thrash and Faerie Fire arrive at 28, so the first two rows below apply from level 28):

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 8 | core | Weakened Blows missing on the target, or three or more enemies within 8 yd | Thrash (106832 cast, 77758 effect, 28) | yes (`BearTankDruidStrategy.cpp:190` at 15.5 for the debuff, `:202` at 26 for three or more; `DruidTriggers.cpp:173-176`), **from 28 outranked by Faerie Fire (gap 1)** |
| 9 | core | Weakened Armor not at three stacks or under 5 s left | Faerie Fire (770, 28), 35 yd | **wrong: cast on every global in Bear Form at relevance 27, above Mangle (`BearTankDruidStrategy.cpp:194-195`, `DruidTriggers.cpp:88-99`)** |
| 10 | good | Mangle's reset proc, or Tooth and Claw empowered Maul | Maul (6807) at once | **missing: no line reads Tooth and Claw (search of the druid files)** |
| 11 | core | after the debuffs and Mangle | Lacerate (33745, 38), keep three stacks, refresh under 6 s | yes (`BearTankDruidStrategy.cpp:187` at 16.0, `DruidTriggers.cpp:141-158`; BT-068 caps at 3) |
| 12 | good | from 44, health in the upper band or a pull of three or more | Barkskin (22812, 44) | yes (`GenericDruidStrategy.cpp:140` at 40 for health 65 to 85 percent, `:115` at 27 for 45 percent and below); the 30 s cooldown with Thick Hide makes this cheap |

Defensives at these levels: none until 44 (derived, see the table at the end).

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 13 | good | start of a pack of three or more, or a hard pull | Berserk (106952, 48): Mangle without cooldown, three targets | partly: the boost strategy casts it when the group strength balance is at or under 50 (`GenericDruidStrategy.cpp:215`, `strategy/triggers/GenericTriggers.cpp:416-424`); then Mangle at 25 during it (`BearTankDruidStrategy.cpp:192-193`), **below Faerie Fire at 27** |
| 14 | core | health at or below 45 percent (derived: 40) | Survival Instincts (61336, 56), 12 s, half damage | yes at 45 and at critical, relevance 91 (`FeralDruidStrategy.cpp:104-110`), falls back to Barkskin (`:27-30`) |
| 15 | good | a party member is dead, mid-fight | Rebirth (20484, 56) | yes (`GenericDruidStrategy.cpp:133-134` at 29); **leaves Bear Form for the 2 s cast** (`DruidActions.cpp:81-85`), the shift back is the "bear form" edge |
| 16 | good | tier 3 talent at 45 | Faerie Swarm / Typhoon / Mass Entanglement | idle (see Talents) |

Not recommended (derived): Nature's Grasp (52). It is cast at relevance 39 whenever the bot is attacked
(`GenericDruidStrategy.cpp:146`); it roots attackers, and Entangling Roots-type effects break on damage, so on a pack
the area damage frees them again. It is in neither guide.

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 17 | good | tier 4 talent at 60 | Soul of the Forest (passive); Force of Nature (106737) if taken | passive taken; Force of Nature has an edge (`GenericDruidStrategy.cpp:213` at 27) |
| 18 | good | an enemy casts, current target | Skull Bash (106839, 64), 13 yd, 15 s | yes, current target only (`BearTankDruidStrategy.cpp:210-211` at 42, `DruidTriggers.h:150`); any casting enemy and healers only in PvP (`DruidActions.cpp:213-216`, `BearTankDruidStrategy.cpp:217-219`); no interrupt before 64 |
| 19 | good | health at or below 50 percent and rage 40 or more (derived: it converts up to 60 rage, so low rage wastes it; it competes with Savage Defense for rage) | Frenzied Regeneration (22842, 68) | **wrong gate: health at or below 65 percent with no rage test (`BearTankDruidStrategy.cpp:181-182`, relevance 27)** |
| 20 | core | health at or below 30 percent, SI used or not ready (derived) | Might of Ursoc (106922, 72) | yes at 25 percent (`DruidTriggers.h:483-487`, `GenericDruidStrategy.cpp:120-121`, relevance 90) |
| 21 | good | the buff is missing, out of combat | Mark of the Wild (1126, 62) | yes (`GenericDruidNonCombatStrategy.cpp:113` at 14); **it shifts the tank to caster form first** (`:33-37`) and nothing returns it to Bear Form before the next pull (P1) |

### Levels 75 to 89

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 22 | good | tier 5 talent at 75 | Mighty Bash (5211, single stun) or Ursol's Vortex | **no line for any tier 5 talent** |
| 23 | good | rage low | Enrage (5229, 76), 60 s | yes (`BearTankDruidStrategy.cpp:227-228`, relevance 27.2, on the boost balance gate) |
| 24 | core | from 80 | Mastery: Nature's Guardian (77494, passive) | passive |
| 25 | good | the party is about to be crowd controlled or needs speed | Stampeding Roar (106898, 84) | no line in the bear strategy (to check the generic and feral lists; found none in the druid files) |

### Level 90

Guide (Method, wowtbc): Berserk and Incarnation on cooldown; Faerie Fire and Thrash maintained; Mangle on cooldown; Maul
on Tooth and Claw or excess rage; Lacerate as filler; Savage Defense proactively, Frenzied Regeneration reactively;
Nature's Vigil in dungeons, Dream of Cenarius (cast Healing Touch on its proc) otherwise. The bots have Nature's Vigil
(`GenericDruidStrategy.cpp:203-204`) but the premade takes Heart of the Wild; no line uses Dream of Cenarius or Healing
Touch procs for the Guardian.

## Cooldowns and defensives

The guides say "hardest moments" or give an order (wowtbc: Might of Ursoc, Survival Instincts, Barkskin; Method names the
same three plus Frenzied Regeneration); the thresholds are derived because dungeon trash needs numbers. Thresholds are the
bot's own health percent.

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Savage Defense | 10 | proactive against physical | rage 60 or more, no Savage Defense up, in melee | **late: rage 80 (`DruidTriggers.h:489-494`)** |
| Barkskin | 44 | third | on cooldown at the start of a pack, and under 70 percent | yes: 65 to 85 percent at 40 and 45 percent and below at 27 (`GenericDruidStrategy.cpp:115`, `:140`) |
| Survival Instincts | 56 | second | below 40 percent | yes at 45 percent, relevance 91 |
| Frenzied Regeneration | 68 | reactive, unmitigated damage | below 50 percent with rage 40 or more | **gate: 65 percent, no rage test** |
| Might of Ursoc | 72 | first | below 30 percent after Survival Instincts | yes at 25 percent |
| Berserk | 48 | on cooldown | at the start of a pack of three or more | on the strength balance, below Faerie Fire |
| Growl | 8 | taunt | an enemy not on the tank, any mob | yes, single target; no area taunt exists |
| Skull Bash | 64 | not given | any casting enemy in reach | current target only in PvE |
| Self heal out of Bear Form | 4 and 26 | (Method: Healing Touch only on a Dream of Cenarius proc) | out of combat only; in combat never while a healer is in the group (derived: leaving Bear Form drops armor and Stamina at the lowest health) | **gap: in combat Rejuvenation at 50 percent and Healing Touch at 30 percent, relevance 34 and 35 (`FeralDruidStrategy.cpp:94-97`, `DruidTriggers.h:498-509`), above every Bear action except the emergency lines** |

## Healer triage and mana (healers only)

Not applicable to a tank. The resource is rage: Bear Form sets it to 10, Mangle gives 5 (7.5 with Soul of the Forest and
Savage Defense known), taking damage and auto attacks give more, Enrage gives some at 76, and rage decays out of
combat. Spends are Savage Defense (60), Frenzied Regeneration (up to 60) and Maul (30). The guides: Savage Defense and
Frenzied Regeneration come before Maul. Mana only matters for caster form (Healing Touch, Rejuvenation, Mark of the Wild,
shifting costs mana), which is why the form-leaving lines above should be rare.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Thrash and Faerie Fire (28), Lacerate (38), Barkskin (44), Berserk (48), Survival Instincts
  and Rebirth (56), Skull Bash (64), Frenzied Regeneration (68), Might of Ursoc (72), Enrage (76), Stampeding Roar (84) and the
  tier 3 to 6 talents are not available in parts of the 15 to 60 band; the lists leave them out below their level.
- Challenging Roar (an area taunt on older versions, a line in the module) has no row in the 5.4.8 data; there is no
  area taunt, only single target Growl.
- "Dire Bear Form" (the form the module's pull and shift code names) does not exist; the form is Bear Form (5487).
- Method puts Faerie Fire and Thrash before Mangle; the data shows both are free refresh lines with no cooldown (see the
  note after the source list), so the lists read them as upkeep.
- Method's Hit 7.5 and Expertise 15 percent are level 90 caps; they do not steer rolls below that.
- Method's Glyph of Stampeding Roar needs a spell learned at 84; Glyph of Rebirth one learned at 56.
- Savage Defense is a rage spend in 5.4.8 (60 rage, 3 charges, 6 s of dodge each); the guides call it "proactive
  mitigation fed by rage", consistent with the data, but give no number.
- The module's rotation comments say Savage Defense at rage 80 (a "MoP rotation row"); the 5.4.8 cost is 60, so the
  threshold is 20 rage too late (`DruidTriggers.h:489`).
- Frenzied Regeneration is learned at 68 here (the module comments say so); the guides use it from 90.

## Gap list (ordered by value for levels 15 to 60)

Config (can go at once):

1. Tier 6 talent: Nature's Vigil instead of Heart of the Wild (the Guardian has a boost line for Nature's Vigil, none for
   Heart of the Wild); level 90 only. Proposed link `200012` (tier 5 unchanged) or `200022` (tier 5 Mighty Bash,
   Method's default, still idle). Low value for 15 to 60.
2. Tier 5 talent: no pick has code; Ursol's Vortex is a Method alternative, so leave it until a line exists.
3. Glyphs: the premade matches both guides; two of the three majors are idle below 56 and 84. Optional: Barkskin (45623) or
   Survival Instincts (45601) for slot 3 or 6 (owner decision, low value).

Rotation (class fixes, wait for the owner's in-game test):

1. **Fixed (compile pending).** **Faerie Fire outranks the whole rotation from level 28** (`BearTankDruidStrategy.cpp:194-195`, relevance 27, and the
   trigger is always true in Bear Form, `DruidTriggers.cpp:88-99`): Mangle (17.5), Lacerate (16.0), Thrash (15.5 and 26)
   and Swipe (25 and 26) all sit below it, and Faerie Fire has no cooldown, so by the engine's order (highest relevance
   first, `strategy/Engine.cpp:192-247`) it is cast on every global and the rest starve. Inference from the numbers;
   confirm in the action log of a debug bot. Fix: cast Faerie Fire only to refresh Weakened Armor (missing, under three
   stacks or under 5 s), exactly as the Balance trigger does (`DruidTriggers.cpp:82-86`).
2. **Fixed (compile pending): "bear form" pre-action, a non-combat shift that stays silent for eating, drinking, resurrecting and the travel forms, and Growl held for its cooldown below level 28.** **Pull pre-action "dire bear form" never resolves** (`DruidPullStrategy.cpp:45-50`, `DruidPullStrategy.h:16`): use
   "bear form" so the tank is in Bear Form (Growl needs it) before the pull and so levels 8 to 27 do not stall from caster
   form; also add a non-combat bear form edge so the tank is back in form after healing or Mark of the Wild. Add a
   Growl-on-cooldown wait or fallback (P4).
3. **Fixed (compile pending): Savage Defense at 60 rage, Maul from 90 (the Tooth and Claw exception is still open).** **Savage Defense gate and Maul threshold** (`DruidTriggers.h:489-494`, `DruidBearActions.cpp:10-13`): Savage Defense
   at 60 rage (the cost) when none is up; Maul only from about 90 rage or when Tooth and Claw has empowered it (no line reads
   that proc).
4. **Fixed (compile pending) for the two in-combat self heals; the out-of-combat party heals are untouched.** **Leaving Bear Form to heal in combat** (`FeralDruidStrategy.cpp:94-97`, relevance 34 and 35): keep it for a solo
   Guardian and for out of combat; with a healer in the group do not leave the form in combat. The party healing edges
   in the non-combat strategy (`GenericDruidNonCombatStrategy.cpp:129-155`) also shift the tank out of form (to check
   whether they include the tank itself) and leave it in caster form for the next pull.
5. **Frenzied Regeneration gate** (`BearTankDruidStrategy.cpp:181-182`): at 50 percent and rage 40 or more instead of 65
   percent with no rage test.
6. **Skull Bash on any casting enemy of the pack in PvE**, not only the current target (`BearTankDruidStrategy.cpp:210-219`,
   `DruidActions.cpp:213-216`).
7. **Berserk on the first pull of a pack of three or more** instead of on the strength balance, and above Faerie Fire
   (`GenericDruidStrategy.cpp:215`, `BearTankDruidStrategy.cpp:192-193`); Incarnation: Son of Ursoc (tier 4) and the tier 5
   talents have no line.
8. **Dead edges:** Challenging Roar at 26.5 (`BearTankDruidStrategy.cpp:209`; no such spell); Nature's Grasp at 39 on every hit
   (`GenericDruidStrategy.cpp:146`, not recommended); Wild Charge bear version to check (`DruidBearActions.h:12-22`).
9. To check live: Faerie Fire cast by id after taking Faerie Swarm; how long the tank spends in caster form at the start of
   each pull (P1); Growl stall length at levels 10 to 27.
