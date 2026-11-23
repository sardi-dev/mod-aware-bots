# Monk: Mistweaver (healer)

- **Status:** draft, checked against 5.4.8 data and against the bots' code by reading (2026-11-10, code read at module
  commit f13624b). Config items 1 and 2 and rotation items 1 to 6 of the gap list are implemented on
  `feat/class-sheet-mistweaver-blooddk` (BT-135, compile pending, tested in the next dungeon run); the rest is
  open. Nothing verified live. Every "bots today" claim below describes the code before that change and was checked by
  reading the call sites unless it says "to check".
- **Sources:**
  - Method, "Mistweaver Monk Class Guide, MoP Classic" (Devorta, Phase 1, level 90 raid and dungeon, 23 July 2025), read
    2026-11-10.
  - wowtbc.gg, "Mistweaver Monk PvE Guide - MoP Classic" (no patch line on the page; the page the module's premade
    comment cites), read 2026-11-10.
  - The server's own data (`data/dbc/`: Spell with its tooltip text, SpellLevels, SpecializationSpells, SkillLineAbility,
    SpellPower, SpellCooldowns, SpellMisc, SpellCastTimes, SpellDuration, SpellEffect, SpellAuraOptions, SpellShapeshift,
    Talent, GlyphProperties; `data/db2/Item-sparse.db2` for glyph item ids) through the data reader and short read-only
    scripts, and the server core's monk spell scripts (`spell_monk.cpp`) for Mana Tea, Uplift, Renewing Mist, Eminence and
    Soothing Mist.
  - Overgear, Icy Veins and Wowhead were not readable, so two guides were compared, not three.
- **Levels covered:** 15 to 60 in detail (5-player dungeons, a human leader), 61 to 90 briefly.

Which source was richer, per topic: **talents** Method (named picks with the situation for each; wowtbc lists every option
per tier and marks only Chi Wave "raid spread"); **glyphs** wowtbc (one clear standard set) against Method (a raid and a
dungeon set with more alternatives, one of them for a spell a Mistweaver does not have); **stat order** Method only (its
list is ambiguous, see Stat order); **opener** wowtbc only; **single target heals** wowtbc (two ordered lists, low and
high damage; Method names the three spells and gives no order); **area healing** Method (six ordered steps; wowtbc merges
area into its high damage list); **cooldown order** wowtbc (seven ordered entries; Method names four and when to use
Mana Tea and Chi Brew); **mana** Method (stance, Mana Tea, Chi Brew) against wowtbc (Mana Tea only); **fistweaving** equal
(wowtbc adds Expel Harm and Crackling Jade Lightning as Chi makers). **Neither guide says anything** about triage order or
thresholds, levels 15 to 60, what to do before a spell is learned, Detox, resurrection, the buff, what to do when nobody is
hurt or how to handle a channel, so those lines are derived from the 5.4.8 data and marked "derived".

Where the guides disagree and the pick: (1) the role of Soothing Mist: Method calls it the main heal, wowtbc lists it
eleventh as a filler. Pick wowtbc for levels 32 and up (data: Surging Mist is level 32, Soothing Mist costs mana for every
second of a channel the bots cannot interrupt); below 32 Soothing Mist is the only direct heal (derived). (2) Tier 2:
Method's dungeon talent is Chi Burst, wowtbc's is Chi Wave: pick Chi Wave (the module has two lines for it and none for Chi
Burst). (3) Tier 3: Method says Chi Brew, with Ascension only for melee play: pick Chi Brew. (4) Cooldown order: wowtbc Revival,
then Life Cocoon; Method lists Mana Tea first: wowtbc's order for saves, Method's rule for mana. (5) Method's dungeon glyphs
include Touch of Karma, a Windwalker spell a Mistweaver does not know here.

## Which strategy a Mistweaver runs

Read from the factory, not assumed:

- The combat engine gets `heal` (`MistweaverMonkStrategy`, `AiFactory.cpp:535-536`), `dps assist` and `cure`
  (`AiFactory.cpp:540`), `boost` (`:316`, every bot), `save mana` (`:576-577`) and, on maps outside the restricted list, `healer
  dps` (`:580-581`). **No `aoe` strategy**: only the brewmaster gets it (`:534`), so Rushing Jade Wind, Spinning Crane Kick
  and Breath of Fire edges (`MonkAoeStrategy`) never run for a Mistweaver. The non-combat engine gets `nc` and `cure` and
  `dps assist` (`AiFactory.cpp:782-788`), no heal edge (see row 21 of the lists).
- `MistweaverMonkStrategy::InitTriggers` (`MonkStrategies.cpp:250-319`) first calls the shared monk edges
  (`MonkStrategy::InitTriggers`, `:137-174`: Spear Hand Strike, Fortifying Brew at `low health`, Invoke Xuen, Chi Brew, Zen
  Sphere, Expel Harm), then adds the stance, the heal chains and Mana Tea.
- **Default actions** (`MonkStrategies.cpp:73-88`), pushed every tick beside the triggers: Chi Wave on the current target 5.6,
  Renewing Mist 5.5, Enveloping Mist 5.4, Surging Mist 5.3, Soothing Mist 5.2, Uplift 5.1, Jab 5.05, melee 5.0. The heal
  names are `HEAL_ACTION`s, and `CastHealingSpellAction::GetTargetName` is `"self target"`
  (`strategy/actions/GenericSpellActions.h:139`): **the default list heals the monk itself, not the group**. Only the
  "... on party" variants aim at `party member to heal`.
- How an action is chosen (read, `strategy/Engine.cpp:150-260`): every active trigger pushes its actions with their numbers,
  default actions are pushed with theirs, the engine takes the highest first and goes down the list when an action is not
  useful or fails. Heal numbers: light 10 to 19, medium 20 to 29, critical 30 to 39, dispel 50 to 52, emergency 90
  (`strategy/Strategy.h:37-51`). The heal target is the group member with the lowest health percent plus a distance penalty
  (`strategy/value/PartyMemberToHealValue.cpp:34-99`), the tank is not preferred. Bands: critical below 25, low below 45,
  medium below 65, almost full below 85 (config `AiPlayerbot.CriticalHealth` and the three after it).
- **Moving and channels** (read): a cast with a cast time is refused while the bot moves
  (`AI/PlayerbotAI.cpp:3829-3830`); **while the bot channels, every other cast is refused**, including the two spells that are
  instant during Soothing Mist (`:3818-3819`, `castFailed("channeling")`). The mid-cast interrupt logic (`:1207-1283`) covers
  only a spell still in the preparing state and only spells with a heal effect, so it never ends a channel, and Soothing Mist
  has an aura effect, not a heal effect. A channel ends only by its 8 seconds, by a movement action
  (`strategy/actions/MovementActions.cpp:255-262`, `:876-883`) or by the shared `cancel channel` action
  (`strategy/actions/CancelChannelAction.cpp:19-27`, registered at `strategy/actions/ActionContext.h:406`, used by the druid,
  priest, mage and warlock strategies, by no monk edge).

## Talents

Tiers and columns are from Talent.dbc (the guides show builds as images or option lists). "Bots today" is
`AiPlayerbot.PremadeSpecLink.10.1 = 001020` (`conf/playerbots.conf.dist:2901`; one digit per tier, the column counted from 0):
Celerity / Chi Wave / Ascension / Ring of Peace / Diffuse Magic / Rushing Jade Wind. A bot below 75 uses the first four.

| Tier (level) | Pick | Alternative and when | Spell id | Bots today (001020) |
|--------------|------|----------------------|----------|---------------------|
| 1 (15) | Celerity (Method, with Chi Torpedo, for stacked raids) | Tiger's Lust (Method, small rooms), Momentum (wowtbc lists). No Mistweaver line casts Roll, Chi Torpedo or Tiger's Lust (Roll is a PvP gap closer for the other two specs), so the choice has no effect | 115173 / 116841 / 115174 | Celerity, same |
| 2 (30) | **Chi Wave** (derived, see below) | Chi Burst (Method: dungeons and stacked groups; no action exists), Zen Sphere (wowtbc lists; edge exists) | 115098 / 123986 / 124081 | Chi Wave, same; two lines use it (default `MonkStrategies.cpp:77`, party edge `:316-318`) |
| 3 (45) | **Chi Brew** (Method: best all-around) | Ascension (Method: only for a healer in melee), Power Strikes (wowtbc lists) | 115399 / 115396 / 121817 | **Ascension, differs**; Chi Brew has an edge (`:163-165`) |
| 4 (60) | Leg Sweep or Ring of Peace by fight (Method) | Charging Ox Wave (wowtbc lists). No edge for any of the three, so the choice has no effect | 119381 / 116844 / 119392 | Ring of Peace, same |
| 5 (75) | **Healing Elixirs** (derived, see below) | Diffuse Magic (Method: the better mitigation), Dampen Harm (wowtbc: burst) | 122280 / 122783 / 122278 | **Diffuse Magic, differs from the pick**; a Method pick, but no Mistweaver line casts it |
| 6 (90) | **Invoke Xuen** (derived, see below) | Rushing Jade Wind (Method: dungeons, small rooms), Chi Torpedo (stacked groups) | 123904 / 116847 / 115008 | **Rushing Jade Wind, differs**; idle for a Mistweaver (no `aoe` strategy) |

Notes on the picks (data, not guide):

- Tier 2, derived: Chi Wave is free (no power row), 15 s cooldown (not the 8 s of some guides), a 40 yd bolt that heals an ally
  or hurts an enemy and bounces up to 7 times, preferring injured allies. The default list casts it on the enemy target
  whenever it is ready, so its bounces heal the group for free; the party edge casts it on the member to heal below 65%.
- Tier 3, derived: Chi Brew gives 2 Chi at once (two charges) and Mana Tea stacks from level 56 (data text). Chi is the
  resource a bot standing at range cannot make otherwise (see Chi). Ascension is +1 maximum Chi, +15% maximum mana (data), a
  passive with no cost to the bot; it is the better pick only if the bot meleed.
- Tier 5, derived: Healing Elixirs needs no action (data text: it heals 15% of maximum health the first time a hit drops the
  monk below 35%, or when a Brew or Tea is drunk while injured). Diffuse Magic (90% less spell damage for 6 s, 90 s) and Dampen
  Harm have no Mistweaver edge; until one exists, the passive is the only tier 5 talent that works. A Diffuse Magic edge for
  a spell-heavy pull would make Method's pick better.
- Tier 6, derived: Invoke Xuen has an edge for every monk spec (`MonkStrategies.cpp:161-162`) and Xuen's damage heals the
  lowest nearby ally (Eminence of Xuen, `spell_monk_eminence_driver`, `spell_monk.cpp:5647-5690`); wowtbc lists it fourth among
  its cooldowns, Method does not mention it. Rushing Jade Wind has no Mistweaver edge, Chi Torpedo none either.

Proposed link (derived): `002001` (Celerity / Chi Wave / Chi Brew / Ring of Peace / Healing Elixirs / Invoke Xuen). Below 75
only the first four digits count, so for the owner's characters the change is tier 3.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and 75;
a configured glyph goes into the first free unlocked slot of its type, in config order (`Factory/BotFactory.cpp:1338-1345`),
so the order of the majors decides what is worn at 25 to 49, 50 to 74 and 75 and up. "Bots today" is
`AiPlayerbot.PremadeSpecGlyph.10.1 = 85696,87887,85695,87889,87890,85699` (`conf/playerbots.conf.dist:2902`).

| Type | Glyph | Item id | Why (one clause) | Bots today |
|------|-------|--------:|------------------|------------|
| Major | Renewing Mists | 85696 | both guides' first major; the mist travels to the furthest injured ally, not the closest (spell text); Renewing Mist is level 42, so idle from 25 to 41 | yes, slot 1 (level 25) |
| Major | Zen Meditation | 85695 | both guides (raid); channel it while moving (spell text); the spell is level 82 and no bot edge uses it | yes, slot 2 (level 50): **idle until 82 and unused after** |
| Major | Surging Mist | 85699 | wowtbc's standard third major; Surging Mist needs no target and heals the lowest ally in range (spell text); level 32 | yes, slot 3 (level 75) |
| Major, derived | Mana Tea | 85692 | instant instead of channeled, two stacks per use, 10 s cooldown (spell text); a channel the module cannot cancel blocks every heal (see Mana); Mana Tea is level 56 | no |
| Major, optional | Detox | 87899 | Detox heals the target for 5% of maximum health when it removes something (spell 146954, effect value); Detox is level 20, so it works from 25 | no |
| Major, optional | Life Cocoon | 87895 | Life Cocoon can be cast while stunned (spell text); level 50 | no |
| Major (Method) | Spinning Crane Kick, Touch of Death | 85697, 85700 | Method's melee picks (full speed in the kick, a Touch of Death without Chi but 2 minutes longer); no value for a bot that never melees | no |
| Major (Method) | Touch of Karma | 87900 | Windwalker spell (spec 269, level 22); a Mistweaver has no Touch of Karma, so the glyph does nothing | no |
| Major (Method) | Fortuitous Spheres | 87896 | a healing sphere when health drops low (spell text); whether it works for a Mistweaver and whether a bot collects the sphere is to check | no |
| Minor | Spirit Roll, Water Roll, Zen Flight | 87887, 87889, 87890 | cosmetic (both guides: the only good minors; Zen Flight is in neither and teaches a flight spell that needs a license) | yes, all three |

Glyph of Uplift (spell 125669, "Uplift costs mana instead of Chi") exists as a spell but has no GlyphProperties row, so it
cannot be worn.

Proposed majors (derived): `85696,87887,85692,87889,87890,85699`, that is Renewing Mists, Mana Tea (instead of Zen Meditation, which
is idle for 32 levels and then has no edge), Surging Mist. Slot 1 stays idle from 25 to 41 as it is today; Glyph of Detox in
slot 1 and Renewing Mists in slot 2 is the alternative if the owner wants something working at 25 to 41.

## Stat order

Method (level 90, the list is ambiguous: "then" between entries, Haste appears twice): Hit or Expertise (it says "not as much a
priority as other secondary stats"), Critical Strike with Haste (crit first, Haste to one or two soft caps), Intellect, Mastery,
Spirit. Haste breakpoints for Renewing Mist ticks: 3145 (a new tick), 4717 (the same without the 5% group Haste buff), 6141
with raid buffs (two new ticks), 9158 (a third, less valuable). wowtbc gives none. The numbers are level 90 and cannot be
reached with the gear of levels 15 to 60, and Hit and Expertise only matter for a healer in melee (data: the stance turns Spirit
into Hit and Expertise at half its value, so a bot needs no gear for them).

For levels 15 to 79 (derived): **Intellect, then Spirit, then Critical Strike, then Haste, and Mastery last**. Reasons from the data:
Mastery: Gift of the Serpent is learned at 80, so Mastery rating does nothing before it; Mana Meditation (level 10) lets 50% of
Spirit regeneration run in combat; Mana Tea stacks double with a chance equal to the spell crit chance (level 56); Critical
Strike also lifts every heal. From 80: Intellect, Critical Strike, Haste (Renewing Mist ticks, Method), Spirit, Mastery. For loot
rolls and quest rewards: **leather with Intellect and Spirit** (Leather Specialization at 50, spec spell 120224); a piece with
Spirit beats the same piece without; skip Agility and Strength pieces; weapon damage is worth almost nothing (the stance sets
attack power from spell power, so a staff or fist weapon is chosen by its Intellect and Spirit).

Bots today (`Mgr/Item/StatsWeightCalculator.cpp:571-581`): Heal Power 1.0, Critical Strike 1.0, Intellect 0.9, Haste 0.9, mana
regeneration 0.9, Spirit 0.5, Mastery 0.35; a one-handed main hand is scaled by 0.8 (`:856-861`). Close to the derived order,
but Spirit sits below Critical Strike and Haste for levels 15 to 59 and Mastery has weight below 80 (gap 14).

## What the specialisation knows, by level (5.4.8 data)

Spec spells are from SpecializationSpells.dbc; class spells (every monk) are the Monk skill line rows (829) of
SkillLineAbility.dbc with their level from SpellLevels.dbc. Costs are percent of base mana (SpellPower.dbc) unless Chi is named;
cast times and cooldowns from the spell data. Every healing spell of the specialisation needs Stance of the Wise Serpent
(SpellShapeshift rows: stance mask 0x80000 on Soothing Mist, Surging Mist, Enveloping Mist, Renewing Mist, Uplift, Thunder
Focus Tea, Mana Tea, Revival, the statue and Life Cocoon); the class spells accept any of the three stances; Chi Wave,
Touch of Death and Provoke need none.

| Level | Spell (id), cost, cast, cooldown |
|------:|----------------------------------|
| 1 | Jab (100780, 8.0% mana in this stance, +1 Chi, melee) |
| 3 | Tiger Palm (100787, 1 Chi, melee) |
| 7 | Blackout Kick (100784, 2 Chi, melee) |
| 10 | Stance of the Wise Serpent (115070: healing done +20%, mana instead of Energy, Hit and Expertise from Spirit, attack power from spell power, **Eminence**), Soothing Mist (115175: 8 s channel, 1 s ticks, 1% mana plus a channel cost, 30% chance per tick of 1 Chi), Mana Meditation (121278), Tiger Strikes (120272) |
| 14 | Provoke (115546, class) |
| 16 | Enveloping Mist (124682: **3 Chi**, 2 s cast, 6 s heal over time, +30% Soothing Mist healing on the target, instant while channeling Soothing Mist by its text; no mana) |
| 18 | Resuscitate (115178, class: 20.1% mana, 10 s cast, out of combat, restores 35% health and mana) |
| 20 | Detox (115450, class: 2.6% mana, 8 s cooldown, Poison and Disease), Internal Medicine (115451: Detox also takes Magic), Muscle Memory (139598: Jab makes the next Tiger Palm or Blackout Kick hit harder and restore mana) |
| 22 | Legacy of the Emperor (115921, class: 1% mana, +5% Strength, Agility, Intellect to the group), Touch of Death (115080, class, 3 Chi, melee) |
| 24 | Fortifying Brew (115203, class: 3 min, 20 s, -20% damage, +20% health) |
| 26 | Expel Harm (115072, class: 2.5% mana here, 15 s, heals the monk, +1 Chi) |
| 30 | tier 2 talent |
| 32 | **Surging Mist** (116694: 8.8% mana, 1.5 s cast, +1 Chi, instant while channeling Soothing Mist by its text), Spear Hand Strike (116705, class, melee interrupt) |
| 34 | Teachings of the Monastery (116645: Tiger Palm grants Vital Mists, up to 5 stacks, each 20% less cast time and cost on Surging Mist; Blackout Kick grants Serpent's Zeal, 30 s, auto-attacks heal 25% of their damage; Spinning Crane Kick heals) |
| 42 | **Renewing Mist** (115151: 5.85% mana, instant, 8 s cooldown, +1 Chi, 18 s heal over time, 2 s ticks, spreads to the closest injured ally up to 3 targets in total) |
| 44 | Paralysis (115078, class) |
| 45 | tier 3 talent, Dematerialize (122464: stunned, you phase out for a time) |
| 46 | Spinning Crane Kick (101546, class: 7.15% mana, 2.25 s channel, heals nearby allies from level 34's Teachings, +1 Chi from 3 targets) |
| 50 | **Life Cocoon** (116849: 4.95% mana, instant, 2 min, 12 s absorb, +50% periodic healing taken), Leather Specialization (120224) |
| 54 | Crackling Jade Lightning (117952, class: 6 s channel, 40 yd, small mana cost, chance of Chi) |
| 56 | **Mana Tea** (115294: stacks from Chi spent, up to 20, 4% mana per stack) |
| 60 | tier 4 talent |
| 62 | **Uplift** (116670: 2 Chi, instant, heals every ally carrying the monk's Renewing Mist) |
| 64 | Healing Sphere (115460, class: 2% mana, ground target, 0.5 s global cooldown) |
| 66 | **Thunder Focus Tea** (116680: 1 Chi, 45 s, off the global cooldown, next Surging Mist heals double or next Uplift refreshes every Renewing Mist, 30 s) |
| 70 | **Summon Jade Serpent Statue** (115313: 15 min, 30 s cooldown, Eminence of the statue, Serpent's Accord) |
| 75 | tier 5 talent |
| 78 | **Revival** (115310: 7.7% mana, instant, 3 min, heals everyone in range and clears Magic, Poison and Disease) |
| 80 | Mastery: Gift of the Serpent (117907: heals sometimes leave a Healing Sphere near an injured ally) |
| 82 | Zen Meditation (115176, class: 8 s, 90% less damage, taken spells at the party redirected, any melee hit on the monk breaks it, 3 min) |
| 87 | Transcendence (101643) |
| 90 | tier 6 talent |

Chi (data): Jab, Renewing Mist, Surging Mist, Expel Harm +1 each; Spinning Crane Kick +1 from three targets; Soothing Mist 30%
per tick for 1; Crackling Jade Lightning a chance; Chi Brew +2. Spenders: Enveloping Mist 3, Uplift 2, Blackout Kick 2, Touch of
Death 3, Tiger Palm 1, Thunder Focus Tea 1. Maximum 4 by the guides' logic (not read from a data row), 5 with Ascension (data text). Every 4 Chi spent gives one Mana Tea stack (from 56),
doubled with a chance equal to the crit chance.

**Eminence** (from the stance at level 10): when the monk deals non-auto-attack damage, the lowest health ally nearby is healed for
25% of the damage (spell text; the script uses 25, `spell_monk.cpp:5675`); the statue (70) repeats it; from 34 Serpent's Zeal
makes auto-attacks heal too. **Serpent's Accord** (statue, 70): when the monk casts Soothing Mist the statue casts it on another
injured ally. Both statue effects depend on the monk dealing damage or channelling Soothing Mist.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived"
comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/` read at
the commit named above; `MS` is `strategy/Classes/monk/MonkStrategies.cpp`, `MA` is `MonkActions.h` or `.cpp`.

### What runs today (verified by reading)

Stance: combat edge at 29 (`MS:257-259`), out of combat at 22 (`MS:330-340`, the spec is read when the strategy is built). Heal
chains (`MS:267-318`): reach the member to heal 40; group heal chain (three or more members at or below 65%, needs five
members near): Revival 37, Thunder Focus Tea 36, Uplift 35; group heal setting (three or more at or below 85%): Renewing Mist on
party 28, Uplift on party 27; protect party member: Life Cocoon 90; critical (lowest below 25%): Life Cocoon 35, Surging Mist 33,
Enveloping Mist 32; low (below 45%): Renewing Mist 24, Enveloping Mist 23, Surging Mist 22; medium (below 65%): Renewing Mist 19,
Chi Wave on party 17, Surging Mist 15; almost full (below 85%): Renewing Mist 13, Zen Sphere 14 (`MS:168-170`); medium mana
(below 40%): Mana Tea 20; statue 21; Chi Brew at Chi 2 or less 21; Invoke Xuen 20; Expel Harm at 65 to 85% own health 10;
Fortifying Brew at own health below 45% 25; Detox 52 (self) and 51 (member) for Disease, Poison and, for a Mistweaver, Magic
(`MS:363-383`); out of combat Resuscitate 40, Legacy of the Emperor 21 and 20 (`MS:326-352`). Group heals need five members near
(`strategy/triggers/HealthTriggers.cpp:29-42`: fewer than five returns false, then three affected members are needed in a party of
five), so **in a group of two to four there is no Revival, Thunder Focus Tea, Uplift or group Renewing Mist at all**.

### Levels 15 to 29

Known: Soothing Mist, Enveloping Mist (16), Resuscitate (18), Detox (20), Legacy of the Emperor (22), Fortifying Brew (24), Expel
Harm (26), the three melee spells. **No Surging Mist (32) and no Renewing Mist (42).** The only Chi makers are Jab (melee), the
Soothing Mist chance and Expel Harm (26).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | always, in and out of combat | Stance of the Wise Serpent (115070, 10); no heal works without it | yes: `MS:257-259`, `MS:330-340` |
| 2 | core | out of combat, the group lacks it, from 22 | Legacy of the Emperor (115921, 22) | yes: `MS:345-352` |
| 3 | core | a member is dead, out of combat, from 18 | Resuscitate (115178, 18) | yes: `MS:326-328` (40), action `MA.h:66-70` (BT-105); the 20.1% mana cost against drinking first is to check |
| 4 | core | a member below 45%, or the tank is hit (derived: the only direct heal until 32, 8 s of healing for 1% mana plus the channel cost, 30% per second to make a Chi) | Soothing Mist (115175, 10) on that member, with a cancel when the target is full or another member drops below 25% | **gap, the biggest for 15 to 31: no party-aimed Soothing Mist** (self only, default 5.2, `MS:81`, `GenericSpellActions.h:139`); no cancel edge |
| 5 | core | Chi 3, a member below 45% (derived: no mana cost) | Enveloping Mist (124682, 16), instant during Soothing Mist by its text | partly: party edges `MS:295` (23) and `MS:290` (32); **the Chi never builds** (no Chi maker aimed at a party member, Surging Mist is level 32); only the self Soothing Mist default makes the occasional Chi |
| 6 | good | a harmful Poison or Disease (and Magic) on a member, from 20, nobody below 45% (derived) | Detox (115450, 20) | **wrong order: 51 and 52 sit above every heal** (`MS:363-383`, `Strategy.h:46`), on any harmful aura with 0.7 s or more left (`AI/PlayerbotAI.cpp:3416-3448`, config `DispelAuraDuration` 700) |
| 7 | good | own health below 45% | Fortifying Brew (115203, 24) | yes: `MS:149-152` (25) |
| 8 | good | own health below 70% (derived: 2.5% mana, instant, 15 s, +1 Chi) | Expel Harm (115072, 26) | partly: only 65 to 85% own health at relevance 10 (`MS:171-173`, `HealthTriggers.h:72-79`); nothing below 65% |
| 9 | core | no group member below 85%, mana above 60% | nothing costly (derived, see Nobody hurt) | **gap:** the default list casts Surging Mist, Soothing Mist and Renewing Mist on the monk itself (see Nobody hurt) |

### Levels 30 to 44

As above, plus Chi Wave (30 talent), Surging Mist and Spear Hand Strike (32), Teachings of the Monastery (34).

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 10 | core | a member below 45%; the tank below 65% (derived) | Surging Mist (116694, 32), 1.5 s cast, 8.8% mana, +1 Chi: the main heal | yes: party edges `MS:289` (33), `MS:296` (22), `MS:301` (15); refused while moving (`PlayerbotAI.cpp:3829-3830`) |
| 11 | core | Chi 3 and the tank or a member below 65% | Enveloping Mist (124682, 16): a mana-free heal and +30% Soothing Mist healing | partly: only after Surging Mist failed at critical (32 under 33), first at low (23 under 24 Renewing Mist, which is unknown before 42, so 23 leads) |
| 12 | good | on cooldown (15 s), from 30, any injured ally in range or a pack in sight | Chi Wave (115098, talent) | yes: party edge `MS:316-318` (17), default on the enemy `MS:77` (5.6) |
| 13 | good | Vital Mists stacks 3 or more (derived: each stack is 20% less cast time and cost) | Surging Mist | not reachable: Tiger Palm is cast only by healer dps, which is off in dungeons |
| 14 | core | the pull | the stance, a drink before the pull, Legacy of the Emperor | yes, shared |

### Levels 45 to 59

As above, plus Renewing Mist (42), Chi Brew (45 talent), Spinning Crane Kick (46), Life Cocoon (50), Crackling Jade Lightning (54),
Mana Tea (56). Method's and wowtbc's first heal, Renewing Mist, exists from 42.

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 15 | core | tank upkeep: always in combat (derived from both guides: Renewing Mist first in wowtbc's lists and step 1 of Method's area list, 8 s cooldown, 18 s, +1 Chi) | Renewing Mist (115151, 42) on the main tank | **gap: no tank-aimed edge**; Renewing Mist goes to the lowest member only (`MS:279`, `:294`, `:300`, `:305`) and is skipped on a member who already has the bot's mist (`CastAuraSpellAction::isUseful`, `GenericSpellActions.cpp:197-206`) |
| 16 | core | a member below 45% | Renewing Mist, Enveloping Mist (3 Chi), Surging Mist, in that order | yes: `MS:294-296` |
| 17 | core | the tank or a member under attack below 25 to 30%, the group has no other save running | Life Cocoon (116849, 50), 2 min | partly: `MS:284` (90) and `MS:288` (35); see Cooldowns |
| 18 | good | Chi 2 or less | Chi Brew (115399, talent 45) | yes: `MS:163-165` (21), `MA.cpp:140-150` |
| 19 | good | mana below 40%, two or more Mana Tea stacks, from 56 | Mana Tea (115294, 56) | yes: `MS:307-308` (20), `MA.cpp:120-127`; unglyphed it is a channel (see Mana) |
| 20 | good | three or more members below 85% in a group of five | Renewing Mist on each, then Uplift (62) | gated on five members near (see above); below 62 Renewing Mist only |
| 21 | good | out of combat, a member below 85% | cheap top-ups (derived: Renewing Mist 5.85% rather than Surging Mist 8.8%), then drink | **gap: no out-of-combat heal edge** (`GenericMonkNonCombatStrategy`, `MS:321-359`: resurrection, stance and buff only) |
| 22 | expert | Spinning Crane Kick heals every ally standing in it (level 34 Teachings, 46) | Spinning Crane Kick (101546, 46) | not for a healer at range: 7.15% mana, a 2.25 s channel that blocks other casts, no `aoe` strategy for a Mistweaver |

### Levels 60 to 74

As above, plus tier 4 talent (60), Uplift (62), Healing Sphere (64), Thunder Focus Tea (66), Jade Serpent Statue (70).

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 23 | good | at least two allies carry the bot's Renewing Mist and a pack hurt them, Chi 2 or more (derived: Uplift heals only Renewing Mist targets) | Uplift (116670, 62) | **gap: no check that a mist is out** (`MA.cpp:15-18` tests Chi only; the group chain's `uplift` at `MS:275` tests nothing), so it can fire with no mist standing |
| 24 | good | the same, the mists about to end | Thunder Focus Tea (116680, 66), then Uplift (refreshes every mist) | yes in the group chain: `MS:274-275` (36, 35), five members near |
| 25 | expert | an emergency on one target, Surging Mist available | Thunder Focus Tea, then Surging Mist (heals double) | missing |
| 26 | good | before the first pull, once | Summon Jade Serpent Statue (115313, 70) | partly: the edge fires on the first combat tick (`MS:313-315`, 21), not before the pull; a self-cast of a ground-target spell is to check live; its two effects need monk damage or a Soothing Mist channel (see Eminence) |
| 27 | good | a Healing Sphere near and a member hurt | walk through it | missing (to check how spheres are made); level 64 Healing Sphere is a ground cast, expert only |

### Levels 75 to 89

As above, plus tier 5 talent (75), Revival (78), Mastery: Gift of the Serpent (80), Zen Meditation (82).

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 28 | good | three or more of five below 45% (derived: tighter than the module's 65%, Revival is a 3 minute cooldown) | Revival (115310, 78) | yes but early: `MS:273` (37) at three below 65%; also clears Magic, Poison and Disease |
| 29 | good | passive | Healing Elixirs (122280, talent 75) | not in the premade (Diffuse Magic, no edge) |
| 30 | expert | a spell-heavy pull, own health above 50% | Diffuse Magic (122783, talent) | missing (no edge) |
| 31 | expert | out of combat, never in a fight | Zen Meditation (115176, 82): any melee hit breaks it | not applicable to a bot at range; no edge |

### Level 90

Both guides: stance, statue, Renewing Mist first, Chi Wave and Chi Burst on cooldown, Uplift with Thunder Focus Tea for the spread,
Enveloping Mist and Surging Mist around Soothing Mist, Mana Tea for mana, Revival and Life Cocoon for saves. Bots: the same
lines as 75 to 89, plus Invoke Xuen at 20 on cooldown (`MS:161-162`) if taken, Rushing Jade Wind never.

## Cooldowns and defensives

Wowtbc orders the cooldowns Revival, Life Cocoon, Chi Brew, Invoke Xuen, Fortifying Brew, Zen Meditation, Diffuse Magic or Dampen
Harm. Method names Mana Tea (low mana), Chi Brew (more stacks and Chi), Revival (save the group), Life Cocoon (save one). The
thresholds below are derived.

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Life Cocoon | 50 | second (wowtbc), single save (Method) | the tank, or the member an enemy within 30 yd is attacking, below 25% (30% for a non-tank); never on a member who has it; one per 2 minutes | edge at 90 on "party member to protect" (tank below 10%, others below 30%, repaired today) **but the action aims at `party member to heal`, the lowest member** (`MA.h:171`, `GenericSpellActions.h:190`); also in the critical chain at 35 on the lowest member of any role, so the first dps below 25% takes it |
| Revival | 78 | first (wowtbc), save the group (Method) | three of five below 45%; also the dispel for a group debuff | yes at three below 65% in a five-man party (`MS:273`) |
| Mana Tea | 56 | when low on mana (Method) | below 40% mana, two or more stacks, the glyph for an instant cast | yes (`MS:307-308`, `MA.cpp:120-127`), unglyphed channel |
| Chi Brew | 45 talent | third (wowtbc), for stacks and Chi (Method) | Chi 2 or less | yes (`MS:163-165`), not in the premade |
| Thunder Focus Tea | 66 | before Uplift (both) | with Uplift when the mists are out; before Surging Mist in an emergency | group chain only (`MS:274`) |
| Fortifying Brew | 24 | fifth (wowtbc) | own health below 45% | yes (`MS:149-152`) |
| Expel Harm | 26 | Chi maker after taking damage (wowtbc) | own health below 70% | 65 to 85% only (`MS:171-173`) |
| Zen Meditation | 82 | sixth (wowtbc) | none: a melee hit breaks it | no edge (right) |
| Diffuse Magic, Dampen Harm | 75 talent | last (wowtbc), Method prefers Diffuse | a spell-heavy pull (Diffuse) | no edge |
| Healing Elixirs | 75 talent | listed (wowtbc) | passive | not taken |
| Invoke Xuen | 90 talent | fourth (wowtbc) | on cooldown | yes (`MS:161-162`) if taken |
| Jade Serpent Statue | 70 | pre-cast (wowtbc, Method) | before the first pull | first combat tick (`MS:313-315`) |
| Detox | 20 | not given | any harmful Poison, Disease or Magic on a member with 2 s or more left, nobody below 45% | yes at 51 and 52, above every heal (`MS:363-383`) |
| Resuscitate | 18 | not given | out of combat, the tank first, after drinking | yes (`MS:326-328`, 40) |
| Touch of Death | 22 | not given | never for a healer (3 Chi are Enveloping Mist's) | the shared `boost` edge (`MS:388-393`, 23) is in the Mistweaver's engine too; it needs melee range, so it rarely fires; to check |

## Healer triage and mana

**Who first, which spell, at what health (derived from both guides' logic and the thresholds):**

1. A member is dead and nobody is fighting: Resuscitate (18), after the drink, the tank first (it cannot be cast in combat).
2. A member under attack below 25% (below 10% if the tank, the module's own protect value): Life Cocoon (50) on the tank, else an instant:
   Enveloping Mist (16) under Soothing Mist or with 3 Chi, Surging Mist (32). Below 32: Soothing Mist on the member.
3. A member below 45%: Surging Mist (32), Enveloping Mist with 3 Chi on the tank (no mana), Renewing Mist (42) on a member without it.
4. The tank's upkeep, always: Renewing Mist (42) on the tank (8 s, +1 Chi), Enveloping Mist when 3 Chi are banked and the tank is
   below 85%, Chi Wave (talent 30) on cooldown.
5. Area: three or more members hurt: Renewing Mist on each (it jumps up to 3), then Uplift (62) with two or more mists out,
   Thunder Focus Tea first when the mists must be refreshed (66), Revival (78) for a wipe.
6. Dispels and top-ups: Detox (20) only when nobody is below 45%, Renewing Mist for a small deficit, Surging Mist for a large one.
Today: the lowest percent first for everyone, no tank upkeep, the dispel above every heal, Life Cocoon at the first member below 25%.

**Single target ladder (wowtbc, kept):** Renewing Mist, then Enveloping Mist to spend Chi and Surging Mist to make it, Soothing
Mist last. **Area ladder (Method, kept):** Renewing Mist on several, Thunder Focus Tea then Uplift, Uplift alone, Chi Wave and
Chi Burst on cooldown. Where Method calls Soothing Mist "the main heal", the data says otherwise for a bot (cost per second, a channel
the bot cannot interrupt, 30% for a Chi per tick); wowtbc is closer to a bot.

**Chi:** make it with the cheap spells (Renewing Mist, Surging Mist, Chi Brew +2; Chi Wave makes none), spend it on
the mana-free Enveloping Mist (3) and Uplift (2). Never sit at the maximum of 4. Bots today: the Chi value gates only
Uplift on party (`MA.cpp:15-18`), Chi Brew (`MA.cpp:140-150`) and Touch of Death; Enveloping Mist is tried without a Chi test and
fails on power.

**Mana, in a fight:** cost table, percent of base mana (data): Surging Mist 8.8, Jab 8.0, Revival 7.7, Spinning Crane Kick 7.15,
Renewing Mist 5.85, Life Cocoon 4.95, Detox 2.6, Expel Harm 2.5, Healing Sphere 2.0, Crackling Jade Lightning 1.57, Soothing Mist
1.0 plus 1.0 channel cost, Legacy of the Emperor 1.0; Enveloping Mist, Uplift, Thunder Focus Tea cost Chi, Chi Wave nothing,
Resuscitate 20.1 (out of combat). Rules: below 60% the shared save-mana rule blocks every heal on a member at 65% or more, because all
monk heals are marked medium efficiency (`strategy/generic/ConserveManaStrategy.cpp:14-56`); at or below 40% Mana Tea with two or more
stacks (`MS:307-308`); out of a fight drink below 50% in a dungeon (config `AiPlayerbot.DungeonDrinkMana = 50`, line 2272) and 15%
elsewhere; a mana potion at the shared mana trigger. Derived: Surging Mist is the dear spell, so prefer Renewing Mist and the Chi
spenders for small deficits; use the glyph so Mana Tea does not block heals. **Mana Tea** unglyphed is a channel: the server script adds
one second per stack to the base duration and takes one stack and 4% mana per half second tick (`spell_monk.cpp:2458-2541`), so a
full stack bar is a long channel in which the bot refuses every cast.

**How to handle Soothing Mist (derived, given the refusals above):**
- Below 32 it is the only direct heal, so it needs a party-aimed action in the low and critical chains, a `cancel channel` edge when
  the target is full health or dead or another member falls below 25% (the druid and priest strategies have the same shape), and no
  movement while it runs (a movement action ends it).
- From 32 use Surging Mist, Renewing Mist and Enveloping Mist with their cast times and never channel Soothing Mist in a dungeon,
  because nothing else can be cast for up to 8 seconds and the instants that are instant during it are refused too (`:3818-3819`).
  Remove the default Soothing Mist (self target) from the list.
- Optional, after a live check that the server zeroes the cast time of Surging Mist and Enveloping Mist while the channel runs (the
  data gives them 1.5 s and 2 s and no data row changes that; only Thunder Focus Tea and the glyphed Surging Mist have "cast
  directly while channelling" code, `spell_monk.cpp:5692-5700`, `:5777-5787`): let the monk's Soothing Mist channel pass the check at
  `PlayerbotAI.cpp:3818` for those two spells, which also makes the statue's Serpent's Accord work. Expert tier.

**Detox:** Poison and Disease from 20; Magic from 20 through Internal Medicine (Mistweaver only). The Magic edges exist
(`MS:376-383`) and depend on the spec read when the strategy is built. Revival (78) also clears all three. Derived rule: dispel
when nobody is below 45%.

**Resuscitate:** out of combat only, 10 s cast, 20.1% mana, 35% health and mana back; no monk combat resurrection exists in 5.4.8.

**Fistweaving (healing through melee damage):** Eminence is a passive of the stance (level 10). It heals 25% of the non-auto-attack
damage the monk deals to the lowest ally nearby; the statue repeats it from 70; Serpent's Zeal (Blackout Kick, 34) adds auto-attack
healing. Rule for a bot (derived): **do not fistweave in a dungeon**. The factory strips `healer dps` on dungeon maps
(`AiFactory.cpp:580-581`, `conf/playerbots.conf.dist:2333-2339`, `AiFactory.cpp:75-78`), a Mistweaver stands at range, and a healer in
melee is in the cleave and area damage the tank is not meant to carry; Jab alone costs 8% mana. Outside dungeons (a lone bot,
questing) `healer dps` casts Blackout Kick, Tiger Palm, Jab and walks into melee when nobody is hurt
(`MS:124-131`, trigger `strategy/triggers/GenericTriggers.cpp:434-465`), which is the guides' fistweaving, with Eminence healing the
monk as it goes. Expert, only with a rule that leaves melee when anyone is below 65%: Crackling Jade Lightning (54) from range as the
Chi and damage filler (a 6 s channel, so it needs the cancel edge).

**Nobody hurt:** in a dungeon the right thing is to do nothing costly: keep Renewing Mist on the tank, Chi Wave on cooldown, then stand
still (derived: guides' filler lists are for raids). What the list does today (`MS:73-88`, read): Chi Wave on the enemy, Renewing Mist on
itself, Enveloping Mist on itself with 3 Chi, Surging Mist on itself, Soothing Mist on itself (an 8 s channel), Uplift. Nothing in
`isUseful` refuses a heal on a full-health target; the save-mana rule zeroes them only at or below 60% mana
(`ConserveManaStrategy.cpp:31-55`). **By reading, a Mistweaver with nobody hurt burns mana down to 60% on itself and can lock itself
in a channel. The ledger row BT-073 says the self heals are "rejected by the resource check"; I found no such check, so this is
to check live in an action log.**

## Differences from the guide (5.4.8 against MoP Classic)

- Both guides are level 90. Enveloping Mist is level 16, Surging Mist 32, Renewing Mist 42, Life Cocoon 50, Mana Tea 56, Uplift 62,
  Thunder Focus Tea 66, the statue 70, Revival 78. Below those the bot makes do: no Renewing Mist before 42, no Surging Mist before 32.
- Method's area list says Uplift resets Renewing Mist durations; the spell text and the server script say only an Uplift under
  Thunder Focus Tea does (`spell_monk_thunder_focus_tea_uplift`, `spell_monk.cpp:3783-3820`).
- Mana Tea stacks come from Chi spent (4 Chi per stack, up to 20) and from Chi Brew, not from time; Method's "when low on mana"
  is the use, not the source.
- Chi Wave has a 15 s cooldown and no cost in the data.
- Soothing Mist's 30% Chi chance is per tick (aura options); a full channel makes about 2 to 3 Chi.
- Method's Touch of Karma glyph is a Windwalker spell. Method's Touch of Death glyph note ("raids only") is not in the data; the
  glyph exists for every monk from level 22.
- Method's Hit and Expertise caps: the stance gives a Mistweaver those from Spirit, a bot needs no gear for them.
- Method's Renewing Mist haste breakpoints and Chi Burst, Chi Torpedo and Rushing Jade Wind lines: no Mistweaver edge casts them.
- The stance weave into Fierce Tiger for a free Rushing Jade Wind (Method) is not implemented and not wanted.
- wowtbc's opener (statue, Renewing Mist, pre-potion, then Jab, Tiger Palm, Jab, Blackout Kick) and its melee lines are for a healer
  in melee; the bots' pre-pull has no statue and no melee in dungeons.
- Glyph of Uplift exists as a spell but has no glyph property row.
- "Instant while channelling Soothing Mist" for Surging and Enveloping Mist is in the spell text; the server code for it was not found
  for the ordinary spells (to check live).

## Gap list (ordered by value for levels 15 to 60 in dungeons)

Config (can go at once):

1. **Done (BT-135), tier 3 only.** Tier 3 talent: Chi Brew (col 2) instead of Ascension, link `002020` (tiers 5 and 6 left as they were: the
   owner kept Diffuse Magic and Rushing Jade Wind for now; Healing Elixirs and Invoke Xuen stay a derived proposal for 75 and 90).
2. **Done (BT-135).** Glyphs: Mana Tea (85692) instead of Zen Meditation (85695, spell level 82, no edge): `85696,87887,85692,87889,87890,85699`.
   Item 85692 is named "Glyph of Mana Tea" in `Item-sparse.db2` and its glyph property (spell 123763) is a major glyph; the
   item's teach-spell link was not read.

Rotation (class fixes, wait for the owner's in-game test):

1. **Done (BT-135).** **No Soothing Mist on a party member; a Mistweaver of levels 16 to 31 cannot heal anyone but itself** (`MS:81`,
   `GenericSpellActions.h:139`): the only party edge that could, Enveloping Mist, needs 3 Chi and no Chi maker is aimed at a party member
   before Surging Mist (32). Add a "soothing mist on party" action in the low and critical chains for levels below 32, with a
   `cancel channel` edge (`CancelChannelAction`). Core.
2. **Done (BT-135), with Renewing Mist and Uplift also gone from the defaults.** **The default list heals the monk itself and can lock it in an 8 s channel** (`MS:73-88`, `GenericSpellActions.h:139`,
   `PlayerbotAI.cpp:3818-3819`): drop the self Soothing Mist, Surging Mist and Enveloping Mist from the defaults, keep Chi Wave and Renewing
   Mist on the tank. To check live first (BT-073 says the self heals are rejected, the reading says they are cast). Core.
3. **Done (BT-135), Enveloping Mist as a "good" line.** **No tank upkeep** (`MS:279`, `:294`, `:300`, `:305` aim at the lowest member): Renewing Mist (42) on the main tank, Enveloping Mist
   on the tank at 3 Chi, built on the shared `BuffOnMainTankTrigger` and `BuffOnMainTankAction` (`GenericTriggers.h:973`,
   `GenericSpellActions.h:438`, used by the priests in `PriestTriggers.h:186-191`). Core.
4. **Done (BT-135); Thunder Focus Tea also waits for 3 Chi and two mists.** Group heals need five members near (`HealthTriggers.cpp:29-42`): no Uplift, Thunder Focus Tea, Revival or group Renewing Mist in a
   party of two to four, and in a party of five three members at or below 65 or 85%; also Uplift fires without a Renewing Mist standing
   (`MA.cpp:15-18`, `MS:275`). Good.
5. **Done (BT-135), Mistweaver only.** Detox at 52 and 51, above every heal except Life Cocoon (`MS:363-383`): silent while the lowest member is below 45%, as the priests
   now do. Good.
6. **Done (BT-135).** Life Cocoon is spent on the first member below 25% of any role (`MS:288`), and the emergency edge aims at the lowest member, not
   the protect value (`MS:284`, `MA.h:171`; BT-129 note): aim at the tank or the protect value, and not on a member who has it. Good.
7. No out-of-combat heal (`MS:321-359`): top up hurt members with Renewing Mist or Surging Mist before drinking; derive the mana
   rule from the drink threshold. Good.
8. Mana Tea unglyphed is a channel the bot cannot leave (`MA.cpp:120-127`, script `spell_monk.cpp:2458-2541`): the glyph (config 2) or cast
   it only when nobody is below 65%. Good.
9. Expel Harm only at 65 to 85% own health (`MS:171-173`): below 70% as a self heal and a Chi maker (the brewmaster has this edge
   now). Good.
10. The statue is cast on the first combat tick, not before the pull, and a self-cast of a ground spell is to check (`MS:313-315`,
    `MA.h:421`); its effects need melee damage or a Soothing Mist channel. To check, good.
11. Thunder Focus Tea before Surging Mist in an emergency, Vital Mists, Zen Sphere vs Chi Burst, Diffuse Magic and Dampen Harm edges, the
    Soothing Mist cast-through (see "How to handle Soothing Mist"): expert, later.
12. Touch of Death in the shared `boost` edge for a healer spends 3 Chi (`MS:388-393`): to check live, melee only.
13. Healing spheres (Mastery at 80, Fortuitous Spheres, Healing Sphere at 64) are not collected or placed: to check how they are made.
14. Stat weights: Spirit 0.5 under Critical Strike and Haste, Mastery 0.35 below 80 (`StatsWeightCalculator.cpp:571-581`): Spirit above
    Critical Strike and Haste for levels 15 to 59, Mastery 0 below 80.
15. To check live: the stance after a resurrection (`MS:257-259` against the pull-free order), Resuscitate against the drink, the
    two instants under Soothing Mist, whether `healer dps` melee outside dungeons leaves melee when a member drops.

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 11 players of this specialisation (rating 2440 to 2723). 2v2: 11 players (rating 2663 to 2899). Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** 3v3 (11 players)
- **BT-324 result:** updated (BT-450); 2v2 agrees
- **Set now:** `PremadeSpecLinkPvp.10.1 = 102201` (was `102001`), `PremadeSpecGlyphPvp.10.1 = 87895,87887,87897,87889,87890,85689`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Tiger's Lust 100% / 100% / 22 of 22 | Tiger's Lust | Tiger's Lust | matches |
| 2 (30) | Chi Wave 100% / 100% / 22 of 22 | Chi Wave | Chi Wave | matches |
| 3 (45) | Chi Brew 100% / 100% / 22 of 22 | Chi Brew | Chi Brew | matches |
| 4 (60) | Leg Sweep 91% / 82% / 19 of 22, Charging Ox Wave 0% / 18% / 2 of 22, Ring of Peace 9% / 0% / 1 of 22 | Ring of Peace | Leg Sweep | changed (10 of 11) |
| 5 (75) | Healing Elixirs 91% / 100% / 21 of 22, Diffuse Magic 9% / 0% / 1 of 22 | Healing Elixirs | Healing Elixirs | matches |
| 6 (90) | Invoke Xuen, the White Tiger 100% / 100% / 22 of 22 | Invoke Xuen, the White Tiger | Invoke Xuen, the White Tiger | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Enduring Healing Sphere (85689) | 100% | 100% | 22 of 22 | yes | yes |
| major | Life Cocoon (87895) | 100% | 100% | 22 of 22 | yes | yes |
| major | Paralysis (87897) | 82% | 100% | 20 of 22 | yes | yes |
| minor | Spirit Roll (87887) | 27% | 45% | 8 of 22 | yes | yes |
| minor | Zen Flight (87890) | 36% | 27% | 7 of 22 | yes | yes |
| minor | Water Roll (87889) | 36% | 9% | 5 of 22 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **What changed (3v3, 11 players):** tier 60 is Leg Sweep (91 percent in 3v3, 82 in 2v2; the rest of 2v2 takes Charging Ox Wave) instead of Ring of Peace; the other five tiers and the three major glyphs (Life Cocoon, Enduring Healing Sphere 100/100, Paralysis 82/100) already matched. The talent spells strategy casts Leg Sweep (BT-322).
- **Left open:** the minor slots (the set holds Water Roll, Zen Flight and Spirit Roll; Spirit Roll leads 2v2 with 45 percent, Water Roll 36 percent in 3v3; no majority anywhere, pooled 36, 32 and 23 percent).

## PvP class package: bots today (BT-657 to BT-662, 2026-11-19)

Status of the digest's section 12 rules after the Mistweaver package (see `docs/pvp/class-packages.md`; all wait for the owner's in-game test):

| Digest rule | Bots today |
|---|---|
| 1 Mana Tea in lockouts | done (BT-660): silenced or quiet, two stacks |
| 2 Renewing and Enveloping Mist upkeep | partly, as before (main tank upkeep, heal target proactive Renewing, BT-135 and BT-157); the kill target is not a target of its own |
| 3 Healing Sphere spot heal | done (BT-657) |
| 4 Thunder Focus Tea then Surging Mist | done (BT-659) |
| 5 Life Cocoon at about half health | done (BT-658) beside the emergency edge |
| 6 Fortifying Brew when stunned | partly: the shared defensive layer casts the catalog tempo row |
| 7 Nimble Brew before the medallion | done for every monk (BT-662) |
| 8 Revival safeguards | not built |
| 9 statue and Transcendence placement | partly (statue trigger exists, no port) |
| 10 Ring of Peace or Leg Sweep by comp | not built (the set holds Leg Sweep, 91 percent of the ladder) |
| 11 Paralysis on an offensive cooldown user | not built |
| 12 Invoke Xuen at the start | as before (the `invoke xuen` edge) |

Set: unchanged. Tiger's Lust now has a rule (BT-661); the glyphs of Life Cocoon and Enduring Healing Sphere (100 percent each) are used by BT-658 and BT-657.
