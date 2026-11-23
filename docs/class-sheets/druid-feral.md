# Druid: Feral (damage, cat form)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). This pass added no
  rotation change. The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Feral Druid Class Guide, MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 22 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the talent names in the text are usable).
  - wowtbc.gg, "Feral Druid PvE Guide - MoP Classic" (no patch line on the page, 2026 footer), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellShapeshift, SpellShapeshiftForm, SpellCategories, SpellCategory;
    `data/db2/Item-sparse.db2` for glyph item ids) through `spec_data.py` and short read-only scripts; the core (the SkyFire core: `SpellInfo.cpp` `CheckShapeshift`).
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, as noted in the skill), Wowhead (needs a browser).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's marks),
  61 to 90 briefly.

Which source was richer: **talents (the locked picks Dream of Cenarius and Soul of the Forest)** Method; **the full option list per
tier** wowtbc; **glyphs** equal and they disagree (below); **stat order** Method (wowtbc has none); **single target** both complete and
different in order; **opener** equal (both start with a pre-cast Healing Touch and Prowl); **area** wowtbc (it gives the 2 to 3 and 4 or
more split, Method says "heavy" and "one or two extra"); **defensives** wowtbc (three, Method names one). Neither guide has leveling
advice, an energy pooling rule, a positioning rule, an interrupt, a word on Skull Bash, what a Feral druid does before Rip (20),
Savage Roar (18), Thrash (28) or Omen of Clarity (38), nor a dungeon pull rule, so those lines are derived from the 5.4.8 data.

The two single target lists differ and I pick as follows:

- **Order of the upkeep and the cooldowns.** Method: Faerie Fire, Savage Roar, Rip, Rake, then Berserk, Tiger's Fury below 35 energy,
  Predatory Swiftness Healing Touch, Omen of Clarity Shred, Ferocious Bite, Thrash. wowtbc: Tiger's Fury below 35 energy, Berserk,
  Force of Nature, Faerie Fire, Savage Roar, the Healing Touch, Omen Shred, Rip, Rake, Thrash on Omen, Ferocious Bite, Thrash.
  They agree on what is cast, not on which comes first. Pick Method's order for the core line (the bleeds and Savage Roar are
  snapshots, so they are the lines that must not lapse) and wowtbc's "Tiger's Fury under 35 energy" as the gate for the cooldown.
- **Faerie Fire.** Both put it first or fourth with "if assigned" (Method: first). In a 5-player group the warrior or paladin tank
  often holds Weakened Armor already; derived: refresh it only when no other source holds three stacks (the module's Balance and Bear
  triggers already do that).
- **Ferocious Bite.** wowtbc: Ferocious Bite replaces Rip as the finisher below 20 percent target health (the spell text says it
  refreshes Rip below 25 percent); Method gives it as "at 5 combo points". Pick wowtbc's rule for the expert line, Method's for core.
- **Glyphs.** Method (dungeon): Stampede, Stampeding Roar, Rebirth, or Dash and Savagery; (raid) Stampeding Roar, Savagery, Cat Form.
  wowtbc: Savagery, Healing Touch, Shred. See Glyphs.

One guide line is a logic problem on this server. Both guides say "Shred as the filler during Berserk, otherwise Mangle". Shred needs
to be behind the target and costs 40 energy against 35 for Mangle (SpellPower data). The bots' Mangle edge is always on (see "How the
bots run this"), so the bots already follow the rule by accident, but they never Shred in Berserk or Tiger's Fury, where the
Glyph of Shred removes the positional need.

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.11.1 = 200222`
(`conf/playerbots.conf.dist:2931`; one digit per tier, column counted from 0): Wild Charge, Ysera's Gift, Faerie Swarm, Force of
Nature, Mighty Bash, Nature's Vigil. For a character below 75 only the first four digits are in effect.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (200222) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Feline Swiftness or Displacer Beast by mobility (Method); Wild Charge in the wowtbc opener | | 131768 / 102280 / 102401 | Wild Charge, same as wowtbc; it has a line (`CatDpsDruidStrategy.cpp:262-263`, `FeralDruidStrategy.cpp:122-132`) |
| 2 (30) | Ysera's Gift (Method lists no Feral pick; wowtbc lists all) | Renewal, Cenarion Ward | 145108 / 108238 / 102351 | Ysera's Gift (passive) |
| 3 (45) | Faerie Swarm, Typhoon or Mass Entanglement by need (Method) | | 106707 / 132469 / 102359 | Faerie Swarm |
| 4 (60) | **Soul of the Forest (Method: "locked DPS pick")** | Force of Nature (wowtbc priority 3 and in its opener), Incarnation | 114107 / 106737 / 106731 | **Force of Nature, differs;** it has a line, behind the boost gate (`GenericDruidStrategy.cpp:212-213`) |
| 5 (75) | by crowd control need (Method): Mighty Bash, Disorienting Roar, Ursol's Vortex | | 5211 / 99 / 102793 | Mighty Bash; no line for any of the three |
| 6 (90) | **Dream of Cenarius (Method: "locked DPS pick")** | Heart of the Wild, Nature's Vigil | 108373 / 108288 / 124974 | **Nature's Vigil, differs and inert for the Feral:** the boost strategy gives it to the Balance and the Guardian only (`GenericDruidStrategy.cpp:186-205`) |

Notes on the picks (data, not guide):

- **Wild Charge (102401)** in Cat Form becomes the leap behind an enemy (49376, "dazing them", 15 s cooldown) through the Cat Form
  Passive 3025; in Bear Form the charge 16979, in Travel Form a forward leap (102417). The bot casts "wild charge" by name and
  `BotOverrideSpell` applies the aura swap (`AI/PlayerbotAI.cpp:3765-3780`), so the cat gets 49376. Feline Swiftness is a passive +15
  percent run speed; Cat Form itself gives +25 percent (aura 113636).
- **Ysera's Gift** heals the druid for 5 percent of maximum health every 5 s, or the most hurt ally at full health (core script
  comment, `spell_druid.cpp` above the script). Passive.
- **Soul of the Forest (114107):** for the Feral, finishing moves grant energy per combo point (spell text; the effect row suggests 4,
  not decoded, to check). A passive that fits the energy budget; no line is needed. **Force of Nature (106737, 3 charges, 20 s
  recharge, Feral override 102703):** treants that melee for 15 s and cast Entangling Roots (spell text); the roots break on damage
  and can pull a pack off the tank, so it is a mixed gift in a dungeon (derived).
- **Dream of Cenarius (108373):** Healing Touch makes the next two melee abilities deal 30 percent more damage (145152). The bots
  cast Healing Touch from the Predatory Swiftness proc only when hurt (gap 8), so the talent would do nothing without a change.
  **Heart of the Wild (108288):** +6 percent Stamina, Agility and Intellect at all times; a passive. **Nature's Vigil (124974):** +12
  percent damage and healing for 30 s, 90 s cooldown; no Feral line.
- **Typhoon** (knockback, 30 s) would scatter a pack: do not pick it for a bot (derived).

Proposed link (derived, two readings): **guide `200021`** = Wild Charge, Ysera's Gift, Faerie Swarm, Soul of the Forest, Mighty
Bash, Dream of Cenarius (the last needs the Healing Touch line of gap 8 to act); **bot-friendly today `200020`** = the same with Heart of
the Wild, a passive that Method does not rank and the bots can use. The tier 4 change is the one that matters below 75 (Soul of the Forest is
a passive, Force of Nature is gated). Owner questions 1 to 3.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and 75 (config
comment `conf/playerbots.conf.dist:2631-2636`); a configured glyph goes to the first free slot of its type, in config order: major,
minor, major, minor, minor, major. "Bots today" is `AiPlayerbot.PremadeSpecGlyph.11.1 = 40923,43332,40914,43316,44922,40901`
(`conf.dist:2932`).

| Type | Glyph | Item id | Why (one clause) | Bots today |
|------|-------|--------:|------------------|------------|
| Major | Savagery | 40923 | both guides; Savage Roar can be cast with no combo points (a short roar, spell 127540); Savage Roar is learned at 18 | slot 1 (level 25), works |
| Major | Healing Touch | 40914 | wowtbc; the glyph shortens the Nature's Swiftness cooldown (glyph text) or Swiftmend (Healing Touch text); the Feral knows neither (SpecializationSpells), so the glyph does nothing for it (derived) | slot 3 (level 50), **inert** |
| Major | Shred | 40901 | wowtbc; Shred has no positional need while Berserk or Tiger's Fury is up (114234); Shred is learned at 16 | slot 6 (level 75), works |
| Major | Stampede | 40906 | Method dungeon; Stampeding Roar outside Bear and Cat Form; the spell is learned at 84 | no |
| Major | Stampeding Roar | 45604 | Method raid and dungeon; wider radius; spell at 84 | no |
| Major | Rebirth | 40909 | Method dungeon, wowtbc optional; more health on the resurrected; Rebirth is learned at 56 | no |
| Major | Cat Form | 67487 | Method raid; +healing received in Cat Form (47180), a free defensive | no |
| Major | Dash | 43674 | Method alternative; shorter Dash cooldown (59219) | no |
| Major, derived | Ferocious Bite | 48720 | the bite heals the druid for a percentage per energy used (67598); a solo leveling help | no |
| Minor | Grace, Aquatic Form, Stars | 43332, 43316, 44922 | no combat effect (both guides: take any; wowtbc names Grace) | yes (slots 2, 4, 5), harmless |

Disagreement: Method lists Stampeding Roar, Savagery and Cat Form for raids and Stampede, Stampeding Roar and Rebirth for dungeons;
wowtbc lists Savagery, Healing Touch and Shred. Only Savagery is on both. Derived proposal (a glyph in a slot should do something
at the level the slot unlocks): **`40923,43332,40901,43316,44922,67487`** (Savagery at 25, Shred at 50 (learned at 16, so it
works from the first day), Cat Form at 75; the Healing Touch glyph drops out because the Feral cannot use it). Owner question 4.

## Stat order

Method: Hit and Expertise up to about 7.5 percent each, Agility, then Mastery, Critical Strike, Haste. wowtbc gives no stat order. The
caps are level 90 numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Agility first** (it is the Feral's attack power in Cat Form: spell text of 768 "Agility increases attack power"; Strength gives
  a druid nothing in 5.x, inference: Cat Form text names Agility only), then **Stamina** as the survival stat.
- Then **Critical Strike** (Primal Fury, learned at 30, gives a combo point on a critical finisher builder; Leader of the Pack at 46
  also feeds on crits), then **Haste** (faster energy and more globals).
- **Mastery last below 80:** Mastery: Razor Claws is learned at 80 (SpecializationSpells), so earlier Mastery rating does nothing.
- Hit and Expertise only near the caps (Method), so far below level 90 they rank by their weight, not above Agility.
- Leather Specialization is learned at 50 (data): leather with Agility is the target slot type. Any usable weapon of a higher level
  is an upgrade; Feral abilities use weapon damage but the form fixes the attack speed (the code already treats the weapon speed as neutral,
  `Mgr/Item/StatsWeightCalculator.cpp:1056-1058`).
- Bots today (`StatsWeightCalculator.cpp:410-421`): Agility 2.2, **Strength 2.4**, Attack Power 1.0, **Armor Penetration 2.3**, Hit 1.9,
  Critical 1.5, Haste 2.1, **Mastery 2.4 at every level**, Expertise 2.1. Against the guide: Strength outranks Agility (a Strength cloak or ring
  would beat an Agility one), Armor Penetration rating no longer exists in 5.x (inference: no spell in the data converts it), and Mastery counts
  below 80 where it is dead.

## What the specialisation knows, by level (5.4.8 data)

A Feral druid knows the Druid class line (SkillLineAbility 798, acquire method 2) plus the Feral rows of SpecializationSpells (spec 103).
Ids are the spells the player casts. Energy costs are read from SpellPower (type 3); mana costs in percent of base mana; cooldowns
from SpellCooldowns by spell id. The cat's global cooldown is 1.0 s, a mana spell's 1.5 s. Energy is capped at 100 and regenerates 10
per second at base (inference from the form). **Berserk halves every Cat Form cost for 15 s.**

| Level | Spell (id) |
|------:|------------|
| 1 | Wrath (5176) |
| 3 | Moonfire (8921) |
| 4 | Rejuvenation (774, 14.5 percent) |
| 6 | **Cat Form (768, 3.7 percent mana; +25 percent run speed)**; **Rake (1822, 35 energy, 15 s bleed, +1 combo point)**; **Mangle (33876, 35 energy, +1 combo point)**; **Ferocious Bite (22568, 25 energy, up to 25 more consumed for up to +100 percent)**; Prowl (5215) |
| 8 | Bear Form (5487), Growl, Maul |
| 10 | **Tiger's Fury (5217, +15 percent damage for 6 s, restores 60 energy at once, 30 s cooldown, not usable during Berserk)**; the specialisation is chosen here |
| 14 | Feral Instinct (16949, passive, stealth detection) |
| 16 | **Shred (5221, 40 energy, behind the target, +1 combo point, more damage against bleeding targets)**; Travel Form (783) |
| 18 | **Savage Roar (52610, 25 energy, +40 percent physical damage, 18/24/30/36/42 s for 1 to 5 combo points, only effective in Cat Form)**; Aquatic Form |
| 20 | **Rip (1079, 30 energy, 16 s bleed, 8 ticks; each Shred, Ravage or Mangle extends it by 2 s up to 6 s extra)** |
| 22 | Remove Corruption (2782); **Swipe (62078, 45 energy, all nearby enemies, +1 combo point on the combo target)** |
| 24 | Dash (1850, +70 percent for 15 s, 3 min cooldown) |
| 26 | **Predatory Swiftness (16974, passive; proc aura 69369: the next Entangling Roots, Healing Touch, Hibernate or Rebirth is instant, free and usable in any form)**; Healing Touch (5185, 28.9 percent, 2.5 s) |
| 28 | **Thrash (cast 106832, cat effect 106830: 50 energy, 15 s bleed, Weakened Blows -10 percent physical damage for 30 s)**; Faerie Fire (770, 7.5 percent mana, 35 yd, Weakened Armor x3 for 30 s) |
| 30 | tier 2 talent; Primal Fury (16961, a crit with a single-target builder gives an extra combo point) |
| 32 | Pounce (9005, 50 energy, stealth only, 4 s stun, bleed) |
| 34 | Nurturing Instinct (33873, passive) |
| 38 | **Omen of Clarity (16864, passive; Clearcasting 16870/135700 makes the next cost-bearing ability free)** |
| 40 | Infected Wounds (48484, passive, slows) |
| 44 | Barkskin (22812, 60 s cooldown, 12 s, all forms) |
| 45 | tier 3 talent |
| 46 | Leader of the Pack (17007, passive, group crit and a heal on crit) |
| 48 | **Berserk (106952; cat effect 106951: -50 percent Cat Form costs for 15 s, 180 s cooldown)** |
| 50 | Leather Specialization (86097, passive) |
| 52 | Nature's Grasp (16689) |
| 54 | Innervate (29166); **Ravage (6785, 45 energy, from stealth and behind the target)** |
| 56 | **Survival Instincts (61336, 180 s cooldown, 12 s, half damage)**; Rebirth (20484) |
| 58 | Flight Form (33943) |
| 60 | tier 4 talent |
| 62 | Mark of the Wild (1126, 5.0 percent mana) |
| 64 | **Skull Bash (106839, 15 s cooldown, charges to the target and interrupts, locks the school, 13 yd)** |
| 66 | Hibernate (2637) |
| 72 | Might of Ursoc (106922) |
| 75 | tier 5 talent |
| 78 | Cyclone (33786) |
| 80 | Mastery: Razor Claws (77493) |
| 82 | Maim (22570, class line) |
| 84 | Stampeding Roar (106898) |
| 87 | Symbiosis (110309) |
| 90 | tier 6 talent |

A Feral druid does not know Starfire, Starsurge, Moonkin Form, Sunfire, Solar Beam, Nature's Swiftness, Swiftmend, Lacerate, Savage
Defense or Frenzied Regeneration (Balance, Restoration and Guardian rows). It does know the class-line Wrath, Moonfire and Hurricane
(42), which the bots never use as a cat.

### Specialisation swaps and form rules

- **Swaps for Feral (SpecializationSpells, spec 103):** none (no row replaces another spell), so the specialisation part of
  `BotOverrideSpell` (`AI/PlayerbotAI.cpp:3738-3753`) changes nothing. **Aura swaps work** (`:3765-3780`): the Feral Overrides Passive
  (106733) turns Incarnation into King of the Jungle (102543), Force of Nature into the Feral treants (102703) and Heart of the Wild
  into 108292; the Cat Form Passive (3025) turns Wild Charge into the Cat leap (49376). **Mangle** and **Swipe** have several rows with the
  same name (33876 cat, 33878 bear, 33917 base; 62078 cat, 106785 unified): the name lookup resolves to the lowest known id and the cast
  path applies the form's own aura swap (`BotOverrideSpell` comment, `AI/PlayerbotAI.cpp:3724-3729`). **Thrash** resolves to the Feral
  spec's 106832 (class-line 77758 and 106830 have acquire method 0 and are not learned): confirmed by the w73/w74 notes in `CatDpsDruidStrategy.cpp:239-243`.
- **No talent override** in the bots' cast path (`OverrideSpellList` is commented out in `SpellInfo.cpp:718-742`): Faerie Swarm
  would act only when the maintainer adds the branch.
- **Which forms allow what** (`SpellInfo::CheckShapeshift`, `SpellInfo.cpp:1195-1245`; form flags from SpellShapeshiftForm.dbc): Cat
  Form has flags 248 (bit 0 clear), so it blocks every spell marked "not while shapeshifted": **Healing Touch, Rejuvenation, Regrowth,
  Mark of the Wild, Revive, Rebirth and Remove Corruption need caster form** unless Predatory Swiftness is up (the proc makes Healing
  Touch, Rebirth, Entangling Roots and Hibernate castable in any form). Faerie Fire, Barkskin, Survival Instincts, Dash, Prowl and all
  cat attacks work in Cat Form. The cat attacks need Cat Form (Stances mask 1): Shred, Rake, Rip, Mangle, Ferocious Bite, Savage Roar,
  Swipe, Thrash. Cat Form itself cannot be cast from Moonkin or Tree of Life (its "not from" mask), so a Balance-to-Feral change passes
  through caster form.
- **What the bots do about it:** the self heals carry a "caster form" prerequisite and a "melee" continuer
  (`FeralDruidStrategy.h:12-70`); the Predatory Swiftness heal has its own action without the shift (`DruidActions.h:53-62`). The bot's
  Cat Form is a combat trigger (`CatDpsDruidStrategy.cpp:218-219`); out of combat nothing shifts it into the form.

## How the bots run this specialisation (read)

- A Feral druid below 10 has no specialisation and runs the Balance set (BT-045). From 10 a Feral without the Thick Hide aura gets the
  `cat` combat strategy (`CatDpsDruidStrategy`, `DruidAiObjectContext.cpp:102`) plus `aoe`, `cc`, `dps assist`, `feral charge`
  (`Factory/AiFactory.cpp:437-446`), the shared `racials`, `chat`, `default`, `cast time`, `potions`, `duel`, `boost` (`:316`) and
  `behind` (a melee non-tank non-healer, `:564-569`). It is melee (`AI/PlayerbotSpec.cpp:75-80`). There is **no `cure`** in combat for a
  Feral (the non-combat `nc` strategy has the party cures). Out of combat: `nc`, `dps assist`
  (`AiFactory.cpp:730-736`), `buff`, the shared food and follow strategies.
- Rank of an action (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, NORMAL 10, HIGH 20, MOVE 30, INTERRUPT 40, EMERGENCY 90; the
  engine takes the highest action that is useful and possible; an action whose spell is unknown is skipped. Health thresholds
  (`conf.dist:670-676`): critical 25, low 45, medium 65. The cat's actions in rank order: Survival Instincts 91 (critical or low
  health), Skull Bash 42 (the target casts), Predatory Swiftness Healing Touch 39.5 (health 35 percent), Barkskin 40 (health 65 to 85
  percent) and 27 (low health), "set behind" 37 (`ACTION_MOVE + 7`, `DruidAiObjectContext.cpp:26-38`), Healing Touch 35 (health 30
  percent) and Rejuvenation 34 (50 percent), Pounce 29.8 (Prowl up), Prowl 29.5 (never active, see below), Wild Charge 29, Rebirth 29,
  Cat Form 28 and Dash 28, Berserk 27.5 (boost), Tiger's Fury 27.2 and 27, Force of Nature 27 (boost), Savage Roar 26, Predatory
  Swiftness Healing Touch 25.2 (health 90 percent), Swipe 25 (3 attackers within 8 yd), Shred 24.5 (Clearcasting), Ferocious Bite
  24 (the target is under 25 percent and under 20,000 health, 1 or more combo points), Rip 23.5, Swipe 23, Ferocious Bite 22.5, Rake 22.3,
  Thrash 22.2, Mangle 22.0, Rake on a second attacker 22, then the defaults: Pounce 5.6, Ravage 5.5, Shred 5.4, Mangle 5.3, Faerie
  Fire 5.0 and melee 5.0.
- **Mangle is the always-on builder.** The Mangle edge is a debuff trigger on an aura called "mangle", which 5.4.8 never puts on the
  target, so it is active on every tick (`DruidTriggers.cpp:227-234`; the w74 comment at `CatDpsDruidStrategy.cpp:233-237`). Rake and
  Thrash were raised just above it. Shred is reached only on a Clearcasting proc (24.5) or when Mangle cannot be cast.
- **There is no energy pooling.** The engine casts the highest action that is affordable now. A 35 energy builder is affordable
  before Thrash (50) or Swipe (45) are: with regeneration near 10 per second the bot spends at 35, so the 45 and 50 energy abilities
  fire only after Tiger's Fury, Clearcasting or a skipped globals (inference from the ranks and costs, not observed; to check live).
- **`boost` fires when the fight is hard or the bot is alone** (`BoostTrigger`, `GenericTriggers.cpp:416-424`): the "balance" value
  (group levels over attacker levels, elite x3, boss x20, `value/AttackerCountValues.cpp:55-110`) must be 50 or less. A solo Feral
  uses Berserk and Force of Nature on every pull; a group of five at level L needs attackers worth 10 L, about 3.3 elites (inference
  from the formula). Tiger's Fury is not behind that gate.
- **The stealth opener is dormant.** `ProwlTrigger::IsActive` (`DruidTriggers.cpp:347-382`) refuses when the bot is in combat, and
  the Prowl edge sits in the combat strategy (`CatDpsDruidStrategy.cpp:188-189`); the combat engine normally starts only once the bot
  is fighting. (The attack action can change the engine just before the first hit, `strategy/actions/AttackActions.cpp:188`; to
  check live whether Prowl ever fires.) In a dungeon the tank pulls and the cat is in combat from the first tick, so Pounce and Ravage
  matter only for a solo leveling Feral.
- **Positioning:** `behind` pushes "set behind" at 37 whenever the bot is not behind the target (a duel or battleground opponent
  excepted), above every attack. With Mangle as the builder (no positional need) the move is only needed for Shred and Ravage.
- **Healing:** the cat leaves Cat Form for Rejuvenation at 50 percent and Healing Touch at 30 percent (`FeralDruidStrategy.cpp:94-97`).
  The guardian rule that keeps the form while a healer is near (BT-139) applies to tanks only (`DruidTriggers.cpp:471-473`), so a
  dungeon cat still shifts out at 50 percent next to a healer.
- The cast check refuses spells with a cast time while moving; every cat ability is instant, so it only matters for Healing Touch
  and the self heals.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived" comes from
the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/strategy/Classes/druid/` at
commit `2b24e7f`; `CAT` is `CatDpsDruidStrategy.cpp`, `FD` is `FeralDruidStrategy.cpp`, `GD` is `GenericDruidStrategy.cpp`, `DT` is
`DruidTriggers.cpp` or `.h`, `NC` is `GenericDruidNonCombatStrategy.cpp`.

### Levels 15 to 29

Known: Cat Form, Rake, Mangle, Ferocious Bite, Prowl (6), Tiger's Fury (10), Shred (16), Savage Roar (18), Rip (20), Swipe and Remove
Corruption (22), Dash (24), Healing Touch and Predatory Swiftness (26), the tier 1 talent. Thrash and Faerie Fire arrive at 28. The kit
is energy-based and nearly free of mana; the only mana costs are Cat Form (3.7 percent), the heals and Faerie Fire.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, in a group or solo | **Cat Form up before the pull** (768, 6) (derived: the shift costs 3.7 percent mana and a global at the start of each pull; the form is +25 percent run speed) | **missing out of combat:** the only edge is the combat trigger at 28 (`CAT:218-219`, `DT.cpp:191`); nothing re-shifts after a heal or a buff before the next pull |
| 2 | core | Savage Roar missing, 1 or more combo points | **Savage Roar** (52610, 18): +40 percent physical damage, 18 to 42 s | yes: 26.0 (`CAT:225-226`, `DT.h:172-176`); any combo count; **recast only when the aura is gone** (no refresh window) |
| 3 | core | 5 combo points, Rip not on the target, the target lives 12 s or more | **Rip** (1079, 20) | yes: 23.5 (`CAT:227-228`; `DruidCatActions.h:92-96` carries the 12 s life test) |
| 4 | core | Rake missing, the target lives 6 s or more | **Rake** (1822, 6) | yes: 22.3 (`CAT:238`, refresh at 1.5 s left, `DT.h:83-91`; `DruidCatActions.h:47-51`) |
| 5 | core | 5 combo points, Rip up or skipped, Savage Roar up | **Ferocious Bite** (22568, 6) | yes: 22.5 (`FerociousBiteTimeTrigger`, `DT.cpp:249-272`: not while Savage Roar has 6 s or less or Rip 2 s or less, because the bot expects a refresh the roar and Rip triggers never make, see gap 5) |
| 6 | core | builder | **Mangle** (33876, 6) (both guides); Shred (5221, 16) only on Clearcasting (38), in Berserk, or with the Glyph of Shred | Mangle 22.0 always on (`CAT:231-232`); Shred 24.5 on Clearcasting (`CAT:192`), default 5.4 |
| 7 | good | energy under 35, Tiger's Fury ready, no Berserk | **Tiger's Fury** (5217, 10) (both guides: restores 60 energy at once, so a cast at 70 energy wastes 30) | **wrong gate:** two edges: "low energy" (under 30) at 27.0 (`CAT:190-191`) and the aura-missing trigger at 27.2 (`CAT:251-252`, `DruidAiObjectContext.cpp:314`) that casts on every cooldown whatever the energy; `CastTigersFuryAction` refuses it in Berserk (`DruidCatActions.cpp:15-18`) |
| 8 | good | Predatory Swiftness proc, hurt | **Healing Touch** (5185, 26) free and instant, any form | yes: 39.5 under 35 percent health, 25.2 under 90 percent (`CAT:212-215`, `DT.cpp:384-388`); wasted when above 90 percent; never aimed at an ally |
| 9 | good | enemy out of melee | Wild Charge (talent 15) 29, Dash (24) 28 | yes (`CAT:262-265`); Dash is cast whenever the target is out of melee (3 min cooldown, so about once per fight series) |
| 10 | good | 4 or more attackers within 8 yd | Swipe (62078, 22) with Savage Roar kept up (wowtbc: 4 or more targets: Savage Roar, Thrash, Swipe) | **threshold 3** and above Rip (`CAT:257-258`, 23; `GD:318-324`, 25), and it also spends the energy Thrash needs |
| 11 | good | 2 or 3 attackers | Rake on every attacker (wowtbc) | yes: 22 (`CAT:259-260`) |
| 12 | good | alone, health under 50 percent | Rejuvenation (774, 4), Healing Touch (5185, 26) under 30 percent | yes: 34 and 35 (`FD:94-97`), after a shift to caster form; **also next to a healer** (gap 9) |
| 13 | expert | a Prowl and a solo target | Prowl (6) then **Pounce** (9005, 32) or Ravage (6785, 54) (Method opener: Savage Roar, Ravage, Faerie Fire, Rake, Tiger's Fury, Berserk) | edges exist (`CAT:159-166, 188-189, 247-248`) but the combat strategy cannot start Prowl (see above) |
| 14 | expert | 5 combo points, 4 or more seconds of Rip left | refresh Rip with the next builder (the builder extends it by 2 s, up to 6 s extra) | not needed: the bot recasts when gone; a gap of up to 2 s per cycle |

Before the spells are learned (derived): at 6 to 15 the whole kit is Rake, Mangle and Ferocious Bite at 5 combo points; at 16 Shred (not
used by the bots unless Clearcasting); at 18 Savage Roar; at 20 Rip. The edges for unknown spells are skipped (Rip at 5 combo points
falls through to Ferocious Bite at 22.5, the gap is one global).

### Levels 30 to 44

As above, plus the tier 2 talent (30), Primal Fury (30), Pounce (32), Omen of Clarity (38), Infected Wounds (40), Barkskin (44), and
Thrash and Faerie Fire from 28.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 15 | core | Thrash missing or under 1.5 s, or 4 or more attackers | **Thrash** (106832, 28): 50 energy, 15 s bleed, Weakened Blows (-10 percent physical damage dealt by the mob, a defensive for the tank) | edge at 22.2 (`CAT:243`, `DT.h:352-358`, never while Prowl is up); **starves**: a 35 energy Mangle is affordable first (see "no pooling") and Swipe at 25 outranks it |
| 16 | core | Clearcasting up (Omen of Clarity, 38) | **Shred** free (5221) (both guides: Shred on an Omen proc; Thrash on Omen when it has under 3 s left) | yes: 24.5 (`CAT:192`); fails when not behind, the next edge takes the tick |
| 17 | good | Weakened Armor not at three stacks or about to end, no other source, a boss or an elite | **Faerie Fire** (770, 28) (Method line 1 "if assigned"; derived: 7.5 percent mana per refresh) | **wrong refresh test:** the trigger checks the 5 min "faerie fire" aura, while Weakened Armor lasts 30 s (`DT.cpp:137-166`); and with the Omen of Clarity passive (38) it returns true on every tick ("fishing for procs") so Faerie Fire fills every empty tick at 5.0 (`CAT:180-182`); neither guide fishes with it |
| 18 | good | a casting enemy | Skull Bash from 64 (none before; derived: the Feral has no interrupt in 15 to 63) | n/a before 64 |
| 19 | good | health 65 to 85 percent in a pull | Barkskin (22812, 44) on cooldown | yes: 40 and 27 (`GD:114-115, 139-140`) |

### Levels 45 to 59

As above, plus the tier 3 talent (45), Leader of the Pack (46), Berserk (48), Nature's Grasp (52), Ravage (54), Survival Instincts
(56), Rebirth (56).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | good | a boss, an elite or 3 or more attackers | **Berserk** (106952, 48) on cooldown (both guides); spend with Shred, Rake, Rip and the finishers at half cost | `boost` at 27.5 (`GD:214-215`), behind the balance gate: never on an ordinary dungeon pull; solo on every pull |
| 21 | core | health 25 to 45 percent | Survival Instincts (61336, 56), then Barkskin | yes: 91 at low and critical health (`FD:104-110`, fallback Barkskin `FD:27-31`) |
| 22 | good | a party member is dead in combat | Rebirth (20484, 56); free and instant with Predatory Swiftness | yes: 29, and 29 with the proc (`GD:133-134`, `CAT:198-199`) |
| 23 | not recommended | being attacked | Nature's Grasp (16689, 52) (derived: the root breaks on damage from a pack and costs a global in cat form) | cast at 39 for every spec (`GD:145-146`) |
| 24 | good | tier 4 talent at 60 | Soul of the Forest (passive) or Force of Nature (106737) | passive taken in the proposal; Force of Nature on `boost` 27.0 (`GD:212-213`) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 25 | core | out of combat, the group lacks it | Mark of the Wild (1126, 62) | yes: 14 and on the party 13 (`NC:112-113, 187-189`); the node shifts to caster form first (`NC:31-45`), and the name is asked from level 10 on |
| 26 | good | any casting enemy of the pack, in reach | **Skull Bash** (106839, 64), 15 s, charges 13 yd | yes on the current target only: 42 (`CAT:205-206`, `DT.h:154-158`); the enemy-caster edge is restricted to pvp (`DruidActions.cpp:224-228`) |
| 27 | not recommended | health under 25 percent | Might of Ursoc (106922, 72): forces Bear Form, which drops the cat's energy and Savage Roar | yes for every spec: 90 (`GD:120-121`) |
| 28 | good | 5 combo points, the target under 25 percent health | **Ferocious Bite** refreshes Rip below 25 percent (spell text); wowtbc: Bite instead of Rip maintenance below 20 percent | the execute edge needs only 1 combo point and a target under 25 percent and under 20,000 health: 24 (`CAT:193-194`, `DT.cpp:274-297`); a 1 point bite wastes the 5 point finisher |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | tier 5 talent: situational | no line for any pick |
| 80 | Mastery: Razor Claws; stat order of Method | passive; the weights give it 2.4 at every level (see Stat order) |
| 82 | Maim (class line): a stun finisher, a player-only line in the module (`CAT:147-148`, `DruidCatActions.cpp:6-13`) | the action refuses a non-player target before it asks for the spell id |
| 84 | Stampeding Roar (Stampede glyph: any form) | no line |
| 90 | Method single target: Faerie Fire, Savage Roar at 5 points, Rip at 5, Rake, Berserk, Tiger's Fury under 35 energy, Predatory Swiftness Healing Touch, Omen Shred, Ferocious Bite, Thrash, Mangle (Shred in Berserk) | all there except the energy gate of Tiger's Fury, the Savage Roar refresh, the Healing Touch use and Thrash uptime; Dream of Cenarius has no line |

## Cooldowns and defensives

The guides say "on cooldown" and give Tiger's Fury a 35 energy gate; the dungeon timing is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Tiger's Fury | 10 | below 35 energy, on cooldown | the same, not in Berserk | every cooldown, any energy |
| Berserk | 48 | on cooldown | a boss, an elite or 3 or more attackers | `boost`, balance gate |
| Force of Nature | talent 60 | wowtbc priority 3 | a boss or an elite; mind the roots | `boost` 27.0, same gate |
| Savage Roar | 18 | upkeep, 5 points | upkeep; refresh at 5 points when 6 s remain | missing-only |
| Dash | 24 | movement | closing on a fleeing or ranged target | whenever out of melee |
| Barkskin | 44 | defensive (wowtbc) | on cooldown when damaged | yes |
| Survival Instincts | 56 | defensive (wowtbc), listed 1 | below 40 percent | yes, 45 percent |
| Might of Ursoc | 72 | defensive (both guides) | not for the cat | yes |
| Predatory Swiftness Healing Touch | 26 | on every proc (for Dream of Cenarius) | self under 90 percent, or an ally under 60 percent | self only |
| Skull Bash | 64 | not named | every interruptible cast in reach | current target |
| Rebirth | 56 | not named (Method: glyph) | a dead member in combat | yes |
| Self heals (Rejuvenation, Healing Touch) | 4, 26 | not named | alone only | always, at 50 and 30 percent |

## Healer triage and mana (not a healer)

Not applicable to triage. Resource: energy (100 maximum, built by waiting, Tiger's Fury +60, Soul of the Forest finishers), combo points
(5, kept on the target), and a small mana pool for Cat Form, Faerie Fire and the heals. Energy costs: Shred 40, Rake 35, Mangle 35,
Rip 30, Ferocious Bite 25 (+25), Savage Roar 25, Swipe 45, Thrash 50, Pounce 50, Ravage 45. The bots have a simple "spend it if
affordable" loop with one derived gap: the costs above 35 never get pooled for (see "How the bots run this"). Mana is only a worry
for the heals and Faerie Fire fishing (line 17).

## Spell names the Feral strategies ask for that a Feral druid does not know

`SpellIdValue` logs `[SID] ... qualifier='<name>' resolved=0` whenever a name matches no known, non-passive spell
(`strategy/value/SpellIdValue.cpp:121-149`) and caches the answer 20 s per name and bot (`:24`). A trigger that tests the spell id asks
too, so a known-spell test alone does not stop the log; the level gate has to be at registration. From the code (inference, not checked
against the log), the names a level 25 Feral asks for and does not know, by source; the "learned at" column is for the Feral:

| Name | Learned at | Asked from |
|------|-----------:|------------|
| `faerie fire` | 28 | `CAT:180-182` (default edge, every tick), `DT.cpp:137-166` |
| `thrash` | 28 | `CAT:243` (the trigger tests the debuff, the action asks the id) |
| `pounce` | 32 | `CAT:159-163` (default edges on "almost full energy" and "combo points not full": every tick) |
| `ravage` | 54 | `CAT:163-166` (same) |
| `berserk` | 48 | `GD:214-215` (`boost`) |
| `survival instincts` | 56 | `FD:104-110` (critical and low health) |
| `barkskin` | 44 | its fallback, `GD:114-115, 139-140` |
| `skull bash` | 64 | `CAT:205-208` (only when an enemy casts) |
| `force of nature` | talent 60, and only that pick | `GD:212-213` |
| `healing touch` | 26 | `FD:96-97` (health under 30 percent) |
| `nature's grasp` | 52 | `GD:145-146` (being attacked) |
| `rebirth` | 56 | `GD:133-134`, `CAT:198-199` |
| `might of ursoc` | 72 | `GD:120-121` |
| `mark of the wild` | 62 | `NC:112-113`, party edge `NC:187-189` |
| `wild charge` | talent 15 | `CAT:262-263` when another tier 1 talent is picked, and below 15 |
| `rip` | 20 | `CAT:227-228`: asked below 20 at 5 combo points |
| `savage roar` | 18 | `CAT:225-226`: asked below 18 |
| `shred` | 16 | `CAT:169-172`, `CAT:192`: asked below 16 |
| `swipe` | 22 | `CAT:257-258`, `GD:320-323` ("medium aoe"): asked below 22 |
| `dash` | 24 | `CAT:264-265`: asked below 24 |

Fix direction: build the edge lists with a level gate (the bot's level against the spell's learn level), keep the talent-only names
behind a "has the talent" test, and delete the Balance-only and Guardian-only edges from the cat paths.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Tiger's Fury arrives at 10, Shred at 16, Savage Roar at 18, Rip at 20, Predatory Swiftness at 26, Thrash and
  Faerie Fire at 28, Omen of Clarity at 38, Berserk at 48, Ravage at 54, Survival Instincts at 56, Skull Bash at 64, Mastery: Razor
  Claws at 80; below those levels the lists above start without them.
- Omen of Clarity is a passive in 5.4.8 and cannot be cast (BT-146); the proc is Clearcasting.
- Insect Swarm, Cower, Claw, Demoralizing Roar, Thorns, Challenging Roar and Dire Bear Form do not exist (BT-086, BT-146); Faerie
  Fire (Feral) is plain Faerie Fire.
- Mark of the Wild is level 62 here.
- Rip is extended by 2 s per Shred, Ravage or Mangle up to 6 s extra (spell text of 1079), so the guides' "refresh when running out"
  is cheaper than it reads; the bots' recast-when-gone loses at most the last tick.
- Method's Hit 7.5 percent and Expertise 7.5 percent are level 90 caps.
- Dash cooldown is 3 minutes in the data (the Glyph of Dash shortens it).
- Overgear, Icy Veins and Wowhead were not readable, so two guides were compared.

## Gap list (ordered)

Config (can go at once):

1. Talent link `AiPlayerbot.PremadeSpecLink.11.1` (`conf/playerbots.conf.dist:2931`) from `200222` to **`200020`** (Wild Charge, Ysera's Gift,
   Faerie Swarm, Soul of the Forest, Mighty Bash, Heart of the Wild) now, **`200021`** (Dream of Cenarius) once the Healing Touch line of gap 8
   exists. Reasons: Soul of the Forest is a passive on Method's locked list; Force of Nature sits behind the balance gate and its treants
   root the target; Nature's Vigil has no Feral line. The comment at `:2928-2929` needs the same change.
2. Glyph order `PremadeSpecGlyph.11.1` (`:2932`) to `40923,43332,40901,43316,44922,67487` (Savagery, Grace, Shred, Aquatic Form, Stars, Cat
   Form): the Healing Touch glyph does nothing for a Feral (no Nature's Swiftness).
3. Stat weights for Feral (`Mgr/Item/StatsWeightCalculator.cpp:410-421`): Strength 0, Armor Penetration 0, Mastery 0 below level 80; keep
   Agility 2.2 (raise it above Hit 1.9 and Expertise 2.1 so Agility ranks first), Critical 1.5, Haste 2.1 (lower than Critical below 90).

Strategy (class fixes, wait for the owner's in-game test), ranked by value for levels 15 to 60:

4. **Tiger's Fury energy gate**: remove the aura-missing edge (`CAT:251-252`, `DruidAiObjectContext.cpp:314`) or add "energy under 40" to it;
   the "low energy" edge (under 30, `CAT:190-191`) is the guide's rule. Every cooldown cast at 60 energy wastes the energy it exists to give.
5. **Savage Roar and the bite guard**: give the Savage Roar trigger a refresh window (`beforeDuration` 6000 and 5 combo points to refresh,
   any combo count when it is gone) so the `FerociousBiteTimeTrigger` guard (roar over 6 s, Rip over 2 s, `DT.cpp:249-272`) matches what is
   refreshed (`DT.h:172-176`); today a full-combo cat idles through the last 6 s of each roar with the combo capped.
6. **Cat Form out of combat**: a non-combat edge from 6 (the pattern of `TankBearFormTrigger`, `DT.cpp:475-501`, silent for eating, drinking,
   travel forms, mounts and resurrection) so the pull does not start with a shift; the same edge restores the form after a heal or a buff.
   Add Travel Form (783, 16) for outdoor travel when the leader runs far ahead and the bot has no mount (levels 16 to 19), then Cat Form
   again (to check how `CheckMountStateAction.cpp:734-752` interacts: it matches only a master already in Travel Form).
7. **Unknown-name spam** (the level 25 table above): level gates on the edges of `CatDpsDruidStrategy`, `FeralDruidStrategy`,
   `GenericDruidStrategy` (boost, barkskin, nature's grasp, rebirth, might of ursoc), the non-combat Mark of the Wild edges, and a "has the
   talent" test on Wild Charge and Force of Nature. Verify with the debug log.
8. **Predatory Swiftness Healing Touch**: when the healer is out of reach or an ally is under 60 percent, aim it at the ally (free and
   instant); with Dream of Cenarius cast it on every proc (the talent then adds 30 percent to the next two abilities). Today it is a self heal
   under 90 percent (`CAT:212-215`).
9. **No self heals out of form next to a healer**: apply the BT-139 test (`GroupHasLivingHealer`, `DT.cpp:55-71`) to the cat's Rejuvenation
   (34) and Healing Touch (35) edges (`FD:94-97`) as for the tank; keep them for the solo bot.
10. **Energy pooling**: a multiplier that blocks Mangle, Shred and Rake when Thrash, Swipe (in a pack), Rip or Savage Roar is wanted and the
    energy is within 15 of its cost, so the 45 and 50 energy abilities get their turn; Thrash above Swipe in a pack. Derived, to be proved
    by a live energy log first.
11. **Swipe for packs**: threshold 4 attackers within 8 yd (wowtbc), Thrash first, Rake on 2 to 3 (`CAT:257-260`, `GD:318-324`).
12. **Faerie Fire**: test Weakened Armor (the way `WeakenedArmorNeedsRefresh` does, `DT.cpp:170-181`) instead of the 5 min aura, only on a boss or
    elite, and drop the "fish for Omen" branch (`DT.cpp:157-162`).
13. **Ferocious Bite execute**: require 4 or 5 combo points and the target under 25 percent (and drop the 20,000 health cap that shuts it off
    for any mob of level 40 or more), `DT.cpp:274-297`.
14. **Boost gate for the Feral** (`GD:207-216`): replace "balance 50 or less" by "the pull target is an elite or a boss, or 3 or more
    attackers" for Berserk and Force of Nature.
15. **Skull Bash on any casting enemy** of the pack in PvE, not only the current target (`CAT:205-208`, `DruidActions.cpp:224-228`).
16. **Positioning**: push "set behind" (37) only when the next builder is Shred or Ravage (Clearcasting, Berserk, Tiger's Fury with the glyph);
    otherwise stay and Mangle (`DruidAiObjectContext.cpp:26-38`, `DruidTriggers.cpp:111-118`). Add Shred in Berserk.
17. **Stealth opener** (expert): move the Prowl edge to the non-combat strategy for a solo leveling Feral only, with Pounce at 32 and Ravage at
    54; in a group the tank pulls and the opener is not worth the delay.
18. Skill tiers: no gating exists in the Feral strategies. Lines marked good and expert above (Tiger's Fury gate, Healing Touch use, Skull Bash on
    any caster, Berserk timing, pooling, the Ferocious Bite rule) are the ones a casual bot drops.
19. To check live: Thrash uptime and Swipe use in a pack of three (energy log); whether Prowl ever fires; the cast of "faerie fire" after Faerie
    Swarm; Shred from "not behind" failing into Mangle; Wild Charge cat version (49376) through the Cat Form aura swap; the Dash waste; whether
    Typhoon 61391 is ever known without the talent.

Owner questions:

1. Tier 4 (level 60): Soul of the Forest (Method's locked pick, a passive) or Force of Nature (current; wowtbc ranks it third; the treants root the
   target).
2. Tier 6 (level 90): Dream of Cenarius (Method's locked pick; needs gap 8), Heart of the Wild (a passive), or Nature's Vigil (current, inert for the
   Feral).
3. Tier 1: Wild Charge (current, has a line, the bot-friendly pick) or Feline Swiftness or Displacer Beast (the guide's mobility picks).
4. Glyphs: the derived order that does something at each unlock level, or Method's dungeon set (Stampede, Stampeding Roar, Rebirth, all idle until 56
   or 84).
5. Savage Roar: at once with any combo count when it is gone (today), or wait for 3 or more points (guide: 5) on trash.
6. Should a Feral next to a living healer stop leaving Cat Form to heal (the guardian rule, BT-139), or keep the solo behaviour everywhere.
7. Should the cat open from Prowl only when solo, or never.

## PvP playbook (arena, 5.4.8)

New section, 2026-11-19. Sources: player guides (5.x and Classic: one commented 3v3 jungle cleave match by a top feral, an opener video of patch 5.4.7, a talent and macro video of patch 5.3, a raid video of patch 5.4 for the cat basics), the 5.x patch notes, the server's own data (`data/dbc/` through `spec_data.py` and short read-only scripts) and the module code read on `main`. Lines say "(guide)" when a player guide gives them, "(adapted)" when they come from the Restoration druid arena material and were checked against the Feral spell list, "(derived)" when they follow from the 5.4.8 data and game knowledge. About 45 percent of the section is sourced, 10 percent adapted, 45 percent derived. Where nothing reliable exists the line says GAP.

Where the spec stands: no Feral druid is in the MoP Classic EU top 100 ladder of 2026-11-18 (96 players in 3v3, 54 in 2v2), a community tier list of July 2025 rates it B in 3v3 and B in 2v2 (the best of the five specs without ladder data). Jungle cleave (hunter, feral, healer) is the comp the guides name.

### Main plan (guide)

1. Stun the damage dealer, Mighty Bash the healer, let the hunter trap the healer off that stun; when it fails, peel and heal until the next go, then push out again with stuns ready.
2. Against teams that rotate defensives, do not tunnel one target: go on the player with no trinket, no defensive stance and no Intervene or Sacrifice partner in range, swap when the target is covered, keep Berserk and a stun for that swap.
3. The bleeds are the engine and keep ticking when the target runs; kick the healer or a caster for a free cooldown.

### Opener (guide, derived)

- Before the first strike: Savage Roar (with the Glyph of Savagery it costs 0 combo points and lasts 12 s) and Tiger's Fury, then Pounce from Prowl (32, 4 s stun and a bleed), Rake, Ravage, Faerie Fire between energy ticks, Mangle for the fifth point, Rip, Thrash (guide). Ravage needs Prowl and a position behind the target unless Incarnation is active (data, to check what the guide's "free Ravage" is).
- Wild Charge (tier 15) plus Savage Roar in one press closes the distance and opens the buff (guide). The module already casts Wild Charge when the target is out of reach (`CatDpsDruidStrategy.cpp:287`).
- Prep room (derived): Mark of the Wild, Prowl in the start room, a first pillar between the start and the enemy casters. GAP: map specific routes.

### Go window (guide, derived)

Trigger: the target has no trinket, no defensive and no Intervene partner near, the healer is trapped, feared or stunned by the partner, own stun and Berserk unused (guide). Order (derived from the spell texts): Tiger's Fury under 40 energy (it cannot be used while Berserk is up), Berserk (15 s, half cost) and Incarnation (30 s: Ravage without position, stealth abilities and Prowl usable in combat), Pounce from the combat Prowl as the first stun, Ravage under Berserk, Rip at five points, Maim (82, 1 to 5 s) as the closing stun. Pounce, Maim and Mighty Bash share the stun category, so the second is half and the third a quarter. Execute: below 25 percent Ferocious Bite refreshes Rip (data, guide).

### Priority outside a window (guide, derived)

| Priority | Tier | Line |
|---|---|---|
| 1 | core | Savage Roar always up (+40 percent physical damage, data); Rake always up; Rip at five points |
| 2 | core | Mangle as the builder (35 energy, no position); Shred only on a Clearcasting proc or under Berserk (40 energy, from behind) (guide) |
| 3 | good | Tiger's Fury when energy is low and Berserk is not up; Thrash on a melee target for Weakened Blows (10 percent less physical damage dealt) or at high energy (guide, data) |
| 4 | good | Skull Bash (64, 15 s, 4 s lock) on a crowd control cast or a heal in the kill window |
| 5 | good | Predatory Swiftness: Healing Touch on self under 60 percent or on the partner, or Entangling Roots on a kiting caster (it no longer makes Cyclone instant since 5.4.0) |
| 6 | expert | Faerie Fire on a stealthed or vanished enemy and on casters (data text: prevents stealth and invisibility) |
| 7 | expert | Ferocious Bite only below 25 percent or to dump points when Rip has time left; never let Rip or Savage Roar drop for an extra Bite (guide) |

Stat order (guides): agility, then mastery (bleeds), hit and expertise to the cap in the raid guide (a missed strike costs combo points); no PvP stat order was given (derived: PvP power from gear, resilience is no longer an item stat from 5.3).

### Crowd control and defensives

- Stuns and kicks: Pounce (stealth), Maim (5 s at five points), Mighty Bash (tier 75, 5 s, 50 s), Skull Bash. Cyclone (78, 1.7 s cast, 6 s, no cooldown for Feral): bait Grounding Totem and Spell Reflection with it, or take the healer or a damage dealer out; a cat must leave Cat Form to cast it (guide, data). Entangling Roots breaks on damage. Typhoon (tier 45) knocks back (knockbacks share one category, 10 s immunity after one). Disorienting Roar is skipped with a hunter or fire mage partner (adapted).
- Defensives: Barkskin (44, 12 s, 20 percent less damage, 60 s, usable stunned) early and high on health, Survival Instincts (56, 12 s, 50 percent less damage, 180 s), Bear Form (frees from roots and snares, +40 percent Stamina for leather wearers since 5.4.0), Cenarion Ward, Heart of the Wild (45 s of better healing), Dash (24) and Stampeding Roar (84) for roots and snares, Displacer Beast kept until the enemy melee used its gap closer (adapted). Glyph of Cat Form (+20 percent healing taken in Cat Form) and Glyph of Barkskin (25 percent fewer crits taken) are the guide's survival glyphs.
- Intervene only works inside a range: a target far from the enemy warrior is a free kill (guide).

### Matchups and partners

Guide: warrior, shaman, paladin team: bait Grounding and Reflect with Cyclone, kick the paladin, go on whoever lacks Intervene, Sacrifice and trinket, double fear from the priest on the shaman and paladin. Partners named: hunter (traps, Binding Shot), shadow priest and mage ("kitty cleave"), Discipline priest (shields, Leap of Faith). Derived: casters and healers: Pounce from stealth, Skull Bash, Mighty Bash. Mirror, rogue and death knight matchups: GAP.

### PvP set (no ladder data)

Premade today: `PremadeSpecLinkPvp.11.1 = 122000` (Displacer Beast, Cenarion Ward, Typhoon, Soul of the Forest, Disorienting Roar, Heart of the Wild) and glyphs `45622,43332,67484,43316,44922,48720` (Cyclone, Grace, Fae Silence, Aquatic Form, Stars, Ferocious Bite); it is the Restoration druid set with Ferocious Bite. The guides prefer Wild Charge (15; the module casts it), Mighty Bash (75) and Incarnation (60); the last two have no bot action yet.

### Differences from the guides (5.4.8)

- Nature's Swiftness (132158, level 30) is a Restoration spell, not a Feral talent; the 5.3 guide builds on it. The Feral tier 30 talents are Ysera's Gift, Renewal and Cenarion Ward.
- Predatory Swiftness no longer makes Cyclone instant; Cyclone has no cooldown for Feral (5.4.0). Heart of the Wild turns only 100 percent of PvP power into healing for Feral while active (5.4.7).
- Faerie Fire lasts 20 s on players in 5.4.0 (the data row shows a 300 s base: to check).
- Level gates: Pounce 32, Berserk 48, Ravage 54, Survival Instincts 56, Skull Bash 64, Cyclone 78, Maim 82, Stampeding Roar 84, Symbiosis 87.

### Bots today (read on `main`, 2026-11-19)

| Rule | Bots today |
|---|---|
| Savage Roar, Rake, Rip upkeep | exists (`CatDpsDruidStrategy.cpp:246,259,249`) |
| Pounce from Prowl | exists (`CatDpsDruidStrategy.cpp:271`; Prowl edge `GenericDruidNonCombatStrategy.cpp:148`); no target choice by trinket or defensive |
| Tiger's Fury at low energy | partly (`:201`; the Berserk exclusion is not checked: to check) |
| Berserk and Incarnation in a go | missing: Berserk waits for an elite, a boss or three attackers (`DruidTriggers.cpp:526-538`); no Incarnation action |
| Skull Bash on a heal or a crowd control cast | exists (`CatDpsDruidStrategy.cpp:222-225`, through the interrupt gate) |
| Maim as a closing stun, Mighty Bash on the healer | partly (Maim `:152`, level 82, no target logic); Mighty Bash missing |
| Cyclone, Entangling Roots on a kiting caster | missing (dungeon crowd control mark only, `GenericDruidStrategy.cpp:242-255`) |
| Typhoon | missing for Feral (the edge sits in a switched off Balance block, `GenericDruidStrategy.cpp:298`) |
| Barkskin, Survival Instincts | exists (`FeralDruidStrategy.cpp:114-120`, `GenericDruidStrategy.cpp:131`); no early use, no pre-swap Bear Form |
| Predatory Swiftness spend | partly (`CatDpsDruidStrategy.cpp:230-236`: heal; no Roots) |
| Faerie Fire on a stealthed enemy | missing (the cat casts it on elites and bosses only, `DruidTriggers.cpp:125-140`) |

### Gap list (ordered by value; rotation items wait for the owner's in-game test)

Config:

1. `PremadeSpecLinkPvp.11.1` tier 15 to Wild Charge (digit 2): the code casts it and the guides pick it (guide, derived). Tiers 60 and 75 stay Soul of the Forest and Disorienting Roar until Incarnation and Mighty Bash have an action.
2. Glyphs: add the Glyph of Savagery and the Glyph of Barkskin in place of Aquatic Form and Stars (guide); item ids to look up.

Rotation:

1. Berserk, Tiger's Fury and a stun in a go against a player, not only on elites (expert).
2. Mighty Bash and Cyclone on the healer in a window (expert); Cyclone on an off damage dealer first (adapted).
3. Typhoon on a melee player in the face: re-enable the edge for Feral (good).
4. Target swap by trinket, defensive and Intervene range (expert).
5. Entangling Roots from a Predatory Swiftness proc on a kiting caster (good).
6. Faerie Fire on a stealth class (good).
