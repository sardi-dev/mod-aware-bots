# Druid: Balance (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). This pass added no
  rotation change. The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Balance Druid Class Guide, MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 22 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the talent names in the text are usable).
  - wowtbc.gg, "Balance Druid Rotation & Talent Build - MoP Classic" (no patch line on the page, 2026 footer), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellShapeshift, SpellShapeshiftForm, SpellCategories, SpellCategory,
    SpellRadius; `data/db2/Item-sparse.db2` for glyph item ids) through `spec_data.py` and short read-only scripts; the core
    (the SkyFire core: `SpellInfo.cpp` `CheckShapeshift`, `scripts/Spells/spell_druid.cpp` for Eclipse, Astral
    Communion and Solar Beam).
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, as noted in the skill), Wowhead (needs a browser).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's
  marks), 61 to 90 briefly.

Which source was richer: **talent picks (a default for the damage tiers)** Method; **the full option list per tier (Wild Charge,
Renewal, Cenarion Ward, Dream of Cenarius)** wowtbc; **glyphs** wowtbc (standard, situational and minor lists; Method names three
majors and calls the minors weak); **stat order (with the haste breakpoints)** Method; **opener, movement rules, Healing Touch pairing,
area threshold (5 or more)** wowtbc; **DoT order at Eclipse entry and the Hurricane rule (Solar Eclipse)** Method. Neither guide has
leveling advice, a mana plan, a Moonkin Form upkeep rule, Innervate use, Solar Beam use, a dungeon pull rule or a word on what a
Balance druid does before Starsurge (12), Moonkin Form (16), Sunfire (18) or Hurricane (42), so those lines come from the 5.4.8 data
and are marked "derived". The guides agree on the core (DoTs up, Starsurge on cooldown, Wrath in and after Solar Eclipse, Starfire in
and after Lunar Eclipse, Incarnation on entering and Celestial Alignment on leaving an Eclipse). They differ on four points and I
pick as follows:

- **First line.** Method: keep Sunfire and Moonfire up, then Starfall; wowtbc: Starfall first, Starsurge before the DoTs. Pick
  Method's order for the core line (the DoTs carry Shooting Stars and the Eclipse bonus), wowtbc's Starfall-first for the expert
  line. Starfall is level 76, so it does not matter below that.
- **Healing Touch before an Eclipse.** wowtbc lists it as a priority line (with Nature's Swiftness); Method does not. It belongs to the
  Dream of Cenarius talent (level 90, spell text of 108373: Healing Touch raises the next Eclipse by a percentage). Not a rule
  below 90 (derived).
- **Area rule.** Method: Hurricane during Solar Eclipse, no number; wowtbc: 2 to 3 targets keep the single target list with the
  Eclipse DoT on all, Hurricane from 5 targets. Pick wowtbc's number (the data below shows why: Hurricane costs half the base mana
  pool).
- **Glyph list.** Method: Stampede, Stampeding Roar, Rebirth. wowtbc: Rebirth, Stampede, Healing Touch (Guided Stars and
  Stampeding Roar situational). No glyph raises Balance damage in either guide. See Glyphs.

Two guide lines have a logic problem on this server and are corrected here. (1) Both guides say "Wrath during and after Solar
Eclipse". Spell text and the core script agree, but the script only moves the bar when the cast matches the bar's direction (see
"How Eclipse works here"), so the rule has to be read as a direction rule, not a list of two spells. (2) Method's stat order names
Haste twice (breakpoints at item 3, plain Haste at item 5); read it as breakpoints that only matter at level 90.

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.11.0 = 100112`
(`conf/playerbots.conf.dist:2926`; one digit per tier, column counted from 0): Displacer Beast, Ysera's Gift, Faerie Swarm,
Incarnation, Ursol's Vortex, Nature's Vigil. For a character below 75 only the first four digits are in effect.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (100112) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Feline Swiftness, Displacer Beast or Wild Charge by movement needs (Method) | Wild Charge (wowtbc lists it) | 131768 / 102280 / 102401 | **Displacer Beast, differs and idle:** no line casts it (and it would put the moonkin into Cat Form) |
| 2 (30) | Ysera's Gift (total healing) or Renewal (instant burst) (Method) | Cenarion Ward (wowtbc lists it) | 145108 / 108238 / 102351 | Ysera's Gift, same (passive) |
| 3 (45) | Faerie Swarm (wowtbc lists it first), Mass Entanglement if a root is needed (Method) | Typhoon | 106707 / 102359 / 132469 | Faerie Swarm, same |
| 4 (60) | **Incarnation (Method raid default)** | Force of Nature (Method: only in pure single target, "debatable"; wowtbc lists), Soul of the Forest | 106731 / 106737 / 114107 | Incarnation, same, **but no line casts the Balance Incarnation** |
| 5 (75) | situational (Method): Disorienting Roar, Ursol's Vortex, Mighty Bash | | 99 / 102793 / 5211 | Ursol's Vortex; no line for any of the three |
| 6 (90) | **Heart of the Wild ("always the best DPS pick", Method)** | Dream of Cenarius (wowtbc opener uses Healing Touch), Nature's Vigil (wowtbc: "easier") | 108288 / 108373 / 124974 | **Nature's Vigil, differs;** it has a line for the Balance (`GenericDruidStrategy.cpp:196-197`, boost) |

Notes on the picks (data, not guide):

- **Feline Swiftness (131768)** is a passive +15 percent run speed. **Displacer Beast (102280)** teleports forward, activates Cat
  Form and speeds up, 30 s cooldown; **Wild Charge** is, for a moonkin, a backward bound usable in combat only (102383, 15 s cooldown;
  the Moonkin Form aura swaps the talent to it); in Cat Form it is the leap behind an enemy (49376), in Bear Form the charge (16979).
  For a Balance bot only the passive does anything today.
- **Ysera's Gift (145108):** the core script heals the druid for 5 percent of maximum health every 5 s, or the most hurt ally when
  the druid is at full health (`spell_druid.cpp`, comment above the script). Passive, so a dungeon bot gets it for free. Renewal
  (instant, 120 s cooldown) and Cenarion Ward have no line.
- **Faerie Swarm (106707):** "replaces Faerie Fire" with a slowing version (spell text). The bots cast spell 770 by name and the
  module has no talent override in `BotOverrideSpell` (`AI/PlayerbotAI.cpp:3730-3780`: specialisation swap and aura swaps only), so
  whether casting 770 still applies Weakened Armor after the talent is **to check live**. **Typhoon (132469, 30 s cooldown)** is a
  knockback in front of the caster; the Balance strategy fires it at relevance 40 whenever an enemy is within melee
  (`CasterDruidStrategy.cpp:160-161`, `GenericDruidStrategy.cpp:295-296`), which in a dungeon scatters the tank's pack. Do not pick
  it for a bot.
- **Incarnation: Chosen of Elune (102560, via the Balance Overrides Passive 106732; 3 min cooldown, 30 s):** +25 percent Arcane and
  Nature damage while an Eclipse is active (effect of 122114) and free shifting into the improved Moonkin Form. The only Incarnation
  action in the module is the restoration tree form (`DruidShapeshiftActions.h:44`, name "incarnation" for Tree of Life), so a
  Balance Incarnation needs a new line (gap 8). **Force of Nature (106737, 3 charges, 20 s recharge, Balance override 33831):** treants
  that cast Wrath every 2 s for 15 s and also cast Entangling Roots (spell text). It has a Balance line (`GenericDruidStrategy.cpp:188-189`,
  boost, 29.0). **Soul of the Forest (114107)** for Balance only improves the next Astral Communion, which no line casts.
- **Heart of the Wild (108288):** +6 percent Stamina, Agility and Intellect at all times (aura 17005); the cast part (6 min cooldown)
  matters little. A pure passive, which is the best kind for a bot. **Nature's Vigil (124974):** +12 percent damage and healing for
  30 s, 90 s cooldown, plus a small heal on damage; the bot line exists but sits behind the boost gate (gap 10). Over a 90 s cycle that is
  at most +4 percent, below the passive 6 percent Intellect.
- **Astral Communion (127663, level 20)** and **Euphoria (81062, level 38)** are not talents; see the level table.

Proposed link (derived, two readings; the tier 5 digit stays because no pick has a line): **guide `000110`** = Feline Swiftness,
Ysera's Gift, Faerie Swarm, Incarnation, Ursol's Vortex, Heart of the Wild (needs the Incarnation line of gap 8 to act);
**bot-friendly today `000210`** = the same with Force of Nature, which has a line. Owner questions 1 to 3.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and 75 (config
comment `conf/playerbots.conf.dist:2631-2636`); a configured glyph goes to the first free slot of its type, in config order: major,
minor, major, minor, minor, major. "Bots today" is `AiPlayerbot.PremadeSpecGlyph.11.0 = 40909,43332,40906,104102,43316,40914`
(`conf.dist:2927`).

| Type | Glyph | Item id | Why (one clause) | Bots today |
|------|-------|--------:|------------------|------------|
| Major | Rebirth | 40909 | both guides; the resurrected player returns with more health; Rebirth is learned at 56 | slot 1 (level 25), **idle until 56** |
| Major | Stampede | 40906 | both guides; Stampeding Roar can be cast outside Bear and Cat Form; Stampeding Roar is learned at 84 | slot 2 (level 50), **idle until 84** |
| Major | Healing Touch | 40914 | wowtbc; the glyph text says it shortens the Nature's Swiftness cooldown (the Healing Touch spell text says Swiftmend, which a Balance druid does not know: to check); Healing Touch is learned at 26, Nature's Swiftness at 30 | slot 3 (level 75), works |
| Major, situational | Stampeding Roar | 45604 | Method; wider radius; spell at 84 | no |
| Major, situational | Guided Stars | 40916 | wowtbc; Starfall only hits targets with your Moonfire or Sunfire (146655), a safety glyph for dungeons; Starfall is level 76 | no |
| Major, derived | Innervate | 40908 | Innervate on another target also feeds the caster at a reduced effect (54832); Innervate is learned at 54; useful once the bot gives it to the healer (gap 9) | no |
| Major, derived | Hurricane | 40920 | Hurricane and Astral Storm slow their victims (54831); Hurricane is level 42 | no |
| Major | Omens | 40899 | not named by a guide; Faerie Fire, Faerie Swarm, Typhoon, Mighty Bash and others grant Eclipse energy outside an Eclipse (54812) | no |
| Minor | Grace, Sprouting Mushroom, Aquatic Form | 43332, 104102, 43316 | wowtbc lists Grace and Sprouting Mushroom; Method says take any; cosmetic or utility only | yes (slots 2, 4, 5), harmless |
| Minor | Stars | 44922 | Moonkin Form looks like Astral Form (114301), cosmetic | no |

Disagreement: Method and wowtbc share Rebirth and Stampede; Method adds Stampeding Roar, wowtbc Healing Touch plus two
situational. No glyph changes damage, so the choice is only about not wearing an idle glyph. Derived proposal for
`PremadeSpecGlyph.11.0` (a glyph in a slot should do something at the level the slot unlocks): **`40914,43332,40908,104102,43316,40909`**
(Healing Touch at 25, Innervate at 50, Rebirth at 75; the minors stay, they are free). The Stampede glyph drops out because its
spell is learned at 84. Owner question 4.

## Stat order

Method: Intellect; Hit, Expertise and/or Spirit up to 15 percent spell hit; Haste only at the Moonfire and Sunfire tick breakpoints
(5273, then 8089 rating, level 90 numbers); Critical Strike; Haste; Mastery. wowtbc gives no stat order. The caps are level 90
numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Intellect first** (the primary stat; Spell Power is not an item stat in 5.x, inference: the data here does not list item stats).
- **Spirit counts as Hit for a Balance druid:** Balance of Power (33596, learned at 10) grants hit equal to any Spirit gained from items.
  So until the hit cap Spirit is worth as much as Hit; after it, nothing (Balance has no Meditation, which is Restoration only, so
  Spirit gives no mana regeneration in combat). Expertise is on Method's line but spells ignore it (inference); skip it.
- Then **Critical Strike** (periodic Moonfire and Sunfire crits trigger Shooting Stars, level 26; Starfire and Starsurge crits extend
  Moonfire, Wrath and Starsurge crits extend Sunfire, by the spell texts), then **Haste** (the Eclipse loop and the DoTs tick faster).
- **Mastery last below 80:** Mastery: Total Eclipse is learned at 80 (SpecializationSpells), so before it a Mastery rating does nothing.
- Leather Specialization is learned at 50, so before 50 a cloth piece with Intellect competes with leather; take the higher Intellect.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:535-546`, shared with the Shadow priest): Intellect 0.3, **Spirit 0.6**, Spell
  Power 1.0, Hit 1.1, Haste 1.0, Critical 0.8, **Mastery 0.87 at every level**. Against the guide: Spirit outranks Intellect two to
  one (a cloth piece with Spirit and no Intellect would beat one with Intellect), Mastery counts below 80 where it is dead, and the hit
  cap check (`:903-940`, `SPELL_HIT_OVERFLOW` 14 in `StatsWeightCalculator.h:25`) reads rating and auras but not Spirit. The mage and
  warlock branches already use Intellect 1.5 and Mastery 0 below 80 (`:523-534`, `:548-559`).

## What the specialisation knows, by level (5.4.8 data)

A Balance druid knows the Druid class line (SkillLineAbility 798, acquire method 2) plus the Balance rows of SpecializationSpells
(spec 102). Ids are the spells the player casts. Costs are percent of base mana (SpellPower); cooldowns are read from SpellCooldowns
by spell id. The global cooldown is 1.5 s. The class line also holds Cat Form, Bear Form and the cat and bear attacks for every
specialisation, but they are not part of the Balance rotation.

| Level | Spell (id) |
|------:|------------|
| 1 | Wrath (5176, 8.8 percent, 2.0 s cast, 40 yd; Nature) |
| 3 | Moonfire (8921, 8.4 percent, instant, 14 s DoT, Arcane) |
| 4 | Rejuvenation (774, 14.5 percent) |
| 6 | Cat Form (768, 3.7 percent), Rake, Prowl, Ferocious Bite, Mangle (class line) |
| 8 | Bear Form (5487), Growl, Maul |
| 10 | **Starfire (2912, 15.5 percent, 2.7 s cast; Arcane)**; **Eclipse (79577)**; Balance of Power (33596, Spirit becomes hit); Natural Insight (112857, bigger mana pool); Entangling Roots (339); the specialisation is chosen here |
| 12 | **Starsurge (78674, 15.5 percent, 2.0 s cast, 15 s cooldown)** |
| 14 | Celestial Focus (84738, less pushback) |
| 16 | **Moonkin Form (24858, 9.3 percent; +10 percent Arcane and Nature damage, raid spell haste +5 percent, more armor)**; Travel Form (783, 5.6 percent, +40 percent outdoors) |
| 18 | **Sunfire (93402, 9.0 percent, instant, 14 s DoT, Nature)**; Aquatic Form (1066) |
| 20 | **Astral Communion (127663, 4 s channel: 25 Lunar or Solar energy per second in the bar's direction)** |
| 22 | Remove Corruption (2782, curses and poisons); Swipe (class line) |
| 24 | Dash (1850, +70 percent for 15 s, 3 min cooldown, Cat Form) |
| 26 | Healing Touch (5185, 28.9 percent, 2.5 s cast); **Shooting Stars (93399; a periodic Moonfire or Sunfire crit can reset Starsurge and make the next one instant)** |
| 28 | **Solar Beam (78675, 16.8 percent, 60 s cooldown, 40 yd, interrupts the target and silences the area for 8 s)**; Faerie Fire (770, 7.5 percent, 35 yd, Weakened Armor x3 for 30 s); Thrash (class effect spells) |
| 30 | Nature's Swiftness (132158); tier 2 talent |
| 34 | Killer Instinct (108299, passive) |
| 38 | **Euphoria (81062: outside an Eclipse your Wrath, Starfire and Starsurge give double energy)** |
| 42 | **Hurricane (16914, 50.3 percent (!), 10 s channel, 8 yd, 1.5 s global)** |
| 44 | Barkskin (22812, 60 s cooldown, 12 s, usable in all forms) |
| 45 | tier 3 talent |
| 48 | Owlkin Frenzy (48393, passive: attacks taken can make you immune to pushback and raise damage) |
| 50 | Leather Specialization (86104, passive) |
| 52 | Nature's Grasp (16689, enemies that strike the druid are rooted) |
| 54 | Innervate (29166, 180 s cooldown, 10 s, mana regeneration) |
| 56 | Rebirth (20484, 600 s cooldown, usable in combat) |
| 58 | Flight Form (33943) |
| 60 | tier 4 talent |
| 62 | Mark of the Wild (1126, 5.0 percent; +5 percent stats to the whole group) |
| 66 | Hibernate (2637) |
| 68 | **Celestial Alignment (112071, 15 s, 180 s cooldown; both Eclipse bonuses at once, Moonfire also applies Sunfire, the bar is frozen)** |
| 72 | Might of Ursoc (106922) |
| 75 | tier 5 talent |
| 76 | **Starfall (48505, 32.6 percent, 10 s, 90 s cooldown; a Lunar Eclipse resets the cooldown)** |
| 78 | Cyclone (33786) |
| 80 | Mastery: Total Eclipse (77492) |
| 82 | Lunar Shower (33605: Moonfire and Sunfire casts add stacking direct damage and cheaper casts); Maim (22570, class line) |
| 84 | Wild Mushroom and Wild Mushroom: Detonate (88747, 88751); Stampeding Roar (106898) |
| 87 | Symbiosis (110309) |
| 90 | tier 6 talent |

A Balance druid does not know Shred, Savage Roar, Rip, Tiger's Fury, Berserk, Skull Bash, Predatory Swiftness, Lacerate or Savage
Defense (Feral and Guardian rows). **Mark of the Wild is learned at 62, not at 1 or 14**, and **Hurricane at 42** (class line).

### Specialisation swaps and form rules

- **Swaps for Balance (SpecializationSpells, spec 102):** none (no row replaces another spell), so the specialisation part of
  `BotOverrideSpell` (`AI/PlayerbotAI.cpp:3738-3753`) changes nothing for the druid. **Aura swaps do work** (`:3765-3780`) and
  matter here: the Balance Overrides Passive (106732) turns Incarnation into Chosen of Elune, Force of Nature into the Balance
  treants (33831) and Heart of the Wild into 108291; Moonkin Form (24905) turns Wild Charge into the backward bound; **Eclipse
  (Lunar) (107095) turns Hurricane into Astral Storm (106996)** while a Lunar Eclipse is up, so the name "hurricane" casts the
  right spell either way. `HurricaneChannelCheckTrigger` only knows the Hurricane ids (`DruidTriggers.cpp:299-305`), so it cannot
  cancel an Astral Storm channel (to check live).
- **No talent override** in the bots' cast path (`OverrideSpellList` is commented out in `SpellInfo.cpp:718-742`): the Faerie Swarm
  and any talent that replaces a spell act only when the maintainer adds the branch.
- **Which forms allow what** (`SpellInfo::CheckShapeshift`, `SpellInfo.cpp:1195-1245`; the form flags come from
  SpellShapeshiftForm.dbc): **Moonkin Form (flags 2241) and Tree of Life (2257) have bit 0 set, so they count as "not shifted"
  for spells with the attribute "not while shapeshifted"**. Healing Touch, Rejuvenation, Regrowth, Mark of the Wild, Rebirth, Revive,
  Remove Corruption and Innervate can therefore be cast **in Moonkin Form**. Cat Form (248), Bear Form (248), Travel and Aquatic Form
  (216) have bit 0 clear and block them. Cat Form and Travel Form also list Moonkin and Tree of Life in their "not from" mask, so
  going from Moonkin to another form passes through caster form (the module's nodes already do that).
- **What the bots do about it:** the action nodes put a "caster form" prerequisite in front of Rejuvenation, Regrowth, Healing Touch
  (and their party versions) for the Balance strategy too (`FeralDruidStrategy.h:12-70`, added to the Balance strategy at
  `CasterDruidStrategy.cpp:135`) and in front of Mark of the Wild, Revive, Rebirth, the cures and the roots
  (`GenericDruidNonCombatStrategy.cpp:31-96`, `GenericDruidStrategy.cpp:83-89`, `DruidActions.cpp:87-97`). `CastCasterFormAction` removes any shapeshift, Moonkin included
  (`DruidShapeshiftActions.cpp:35-47`). For a moonkin that throws the form away for no reason (gap 5): every self heal or buff costs
  Moonkin Form again (9.3 percent mana and a global) at the next Wrath.

### How Eclipse works here (data and the core script)

- The bar runs from -100 (Lunar) to +100 (Solar) (`POWER_ECLIPSE`). **Starfire gives +20 toward Solar, Wrath 15 toward Lunar,
  Starsurge 20 in the bar's current direction, Astral Communion 25 per second for 4 s**; Euphoria doubles the first three while no
  Eclipse is up (effect data of 2912, 5176, 78674, 127663, 81062; the script comments say 13 and 15, the data rows say 15 and 20).
- At +100 **Eclipse (Solar) 48517** starts (+15 percent Nature damage, +15 percent spell haste for 15 s through Nature's Grace 16886, mana through spell 81070; the size of the mana
  return is not decoded here, to check live) and the direction marker 67484 is set (the bar now points toward Lunar). At -100
  **Eclipse (Lunar) 48518** starts (+15 percent Arcane) with marker 67483 (points toward Solar). The aura ends when the bar crosses 0
  (`spell_druid.cpp:276-313`).
- **A cast only moves the bar when it matches the direction:** Wrath only if the marker points Lunar (or the bar is exactly 0),
  Starfire only if it points Solar (or 0) (`spell_dru_eclipse_energize`, `spell_druid.cpp:477-557`). Casting the other spell gives
  damage but no energy. So the rule is **Starfire unless the marker points Lunar, then Wrath**: in a Solar Eclipse Wrath carries
  the bar down through 0 and on to Lunar; in a Lunar Eclipse Starfire carries it up. A Solar Eclipse lasts about 7 Wraths, a Lunar
  Eclipse about 5 Starfires; between them Euphoria shortens the walk.
- The bar is **not reset when combat ends**, only at death, resurrection and login (`Player.cpp:2408, 5419, 13039`), so there is no
  need to rebuild an Eclipse at each pull; the direction just carries on. Celestial Alignment freezes the bar (the script returns
  while it is up).

## How the bots run this specialisation (read)

- A Balance druid (and a druid with no specialisation, levels 1 to 9, BT-045) gets the `balance` combat strategy
  (`CasterDruidStrategy`, `DruidAiObjectContext.cpp:109`) plus `cure`, `aoe`, `cc`, `dps assist` (`Factory/AiFactory.cpp:415-427`)
  and the shared `racials`, `chat`, `default`, `cast time`, `potions`, `duel`, `boost` (`:316`). It is a ranged bot
  (`AI/PlayerbotSpec.cpp:65-100`). Out of combat: `nc` (`GenericDruidNonCombatStrategy`), `dps assist`, `cure` (`AiFactory.cpp:748-749`),
  `buff` (Mark of the Wild on the party) and the shared food and follow strategies.
- Rank of an action (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, NORMAL 10, HIGH 20, INTERRUPT 40, EMERGENCY 90; the engine
  takes the highest action that is useful and possible; an action whose spell is unknown is skipped. Health thresholds
  (`conf.dist:670-676`): critical 25, low 45, medium 65; mana: low 15, medium 40, high 65. The Balance actions in rank order:
  Might of Ursoc 90 (health 25 percent), Typhoon 40 (enemy within melee), Barkskin 40 (health between 65 and 85 percent) and 27 (low
  health), flee 39 (the target is within melee reach, about 5 yd, and not on the bot), Nature's Grasp 39 (being attacked), Faerie Fire 29.5, Rebirth 29,
  Innervate 29 (medium mana), Nature's Swiftness 29, Force of Nature 29 and Celestial Alignment 29 and Nature's Vigil 28.9 (all
  `boost`), Rejuvenation 28 (health 50 percent) and Healing Touch 28.1 (health 30 percent), Starfall 28.5, Hurricane 23 (3 attackers
  within 8 yd), Eclipse (Solar) Wrath 20 and Eclipse (Lunar) Starfire 20, Sunfire 18, Moonfire 17.5, Starsurge 17, then the defaults:
  Wrath 5.5 (marker points Lunar), Starfire 5.4, Wrath 5.3.
- **`boost` fires when the fight is hard or the bot is alone** (`BoostTrigger`, `GenericTriggers.cpp:416-424`): the "balance" value
  must be 50 or less. It is the group's summed levels (average level times the member count, up to 10) over the attackers' summed
  levels (elite x3, boss x20) (`value/AttackerCountValues.cpp:55-110`). With no group the player side is 0, so a **solo Balance
  druid uses Force of Nature and Celestial Alignment on every pull**; a group of five at level L needs attackers worth 10 L, that is
  3.3 elites of the same level or a world boss (inference from the formula; the number of elites per dungeon pull is to check live).
  So in an ordinary 5-player pull only the edges that are not `BoostTrigger` (Nature's Swiftness) fire.
- The cast check refuses any spell with a cast time while the bot moves (`AI/PlayerbotAI.cpp:3849-3850`, `:3921-3922`), computed
  from the spell's own base cast time, not from procs: **a Starsurge made instant by Shooting Stars is refused while moving**. Moonfire,
  Sunfire and Starfall (no cast time) go through; Hurricane (a channel with a 10 s duration) does not.
- A group Starfall or Hurricane is an area action: braked while any attacker's threat for the bot is 50 percent of the tank's or
  more, and every damage spell at 80 percent (`strategy/generic/ThreatStrategy.cpp:20-30`).
- The moonkin shifts into form only as the prerequisite of its first spell (Moonkin Form sits in the nodes of Wrath, Starfire, Moonfire,
  Sunfire, Starsurge, Starfall, Typhoon, `CasterDruidStrategy.cpp:12-130`); nothing shifts it before the pull.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived" comes from
the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/strategy/Classes/druid/` at
commit `2b24e7f`; `CD` is `CasterDruidStrategy.cpp`, `GD` is `GenericDruidStrategy.cpp`, `DT` is `DruidTriggers.cpp` or `.h`,
`NC` is `GenericDruidNonCombatStrategy.cpp`.

### Levels 15 to 29

Known: Wrath, Moonfire, Starfire (10), Eclipse (10), Starsurge (12), Moonkin Form (16), Sunfire (18), Astral Communion (20), Remove
Corruption (22), Healing Touch and Shooting Stars (26), Solar Beam and Faerie Fire (28), the tier 1 talent. The kit is mana-hungry
(Starfire 15.5 and Starsurge 15.5 percent of base mana per cast) and has no mana tool until Innervate (54): drink at 50 percent
(`DungeonDrinkMana`, `conf.dist:2272`). Before 12 the whole kit is Wrath, Moonfire and (from 10) Starfire; before 16 there is no form.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, from 16 | **Moonkin Form up before the pull** (24858, 16) (derived: otherwise the first global of every fight is a 9.3 percent shift; the form is +10 percent damage and +5 percent raid haste) | **missing out of combat:** only as a prerequisite of the first spell (`CD:12-130`); nothing shifts back after a heal or a buff (gaps 5 and 6) |
| 2 | core | target will live 8 s or more | Moonfire (8921, 3), refresh when it is gone | yes: 17.5 (`CD:175-176`, `DT.h:111-115`); the trigger is `DebuffTrigger` with no refresh window, so it recasts only when the aura is gone |
| 3 | core | from 18, same | Sunfire (93402, 18), refresh when it is gone | yes: 18.0 (`CD:173-174`; the trigger named "insect swarm" tracks Sunfire, `DT.h:97-101`); asks the unknown name below 18 |
| 4 | core | off cooldown, from 12 | Starsurge (78674, 12): 15 s cooldown, instant on a Shooting Stars proc (26) | yes: 17.0 (`CD:180-181`, `DT.h:469-473`); refused while moving even when instant (see above) |
| 5 | core | from 10, the bar points Solar or is at 0 | **Starfire** (2912, 10) | yes: default 5.4 (`CD:146`) |
| 6 | core | from 10, the marker points Lunar (this includes every Solar Eclipse) | **Wrath** (5176, 1) | yes: 5.5 (`CD:194-195`, trigger `DT.cpp:82`) and 20 in Solar Eclipse (`CD:188-189`); default 5.3. Starfire 20 in Lunar Eclipse (`CD:190-191`) |
| 7 | good | a casting enemy, from 28 | **Solar Beam** (78675, 28) on the current target (derived: a 60 s interrupt that also silences the area for 8 s; a Balance druid has no other interrupt in 15 to 60) | **missing:** no line (searched the druid files) |
| 8 | good | party member with a curse or poison, from 22 | Remove Corruption (2782, 22) | yes: `cure` strategy (`GD:149-169`); cast from Moonkin only after leaving the form (gap 5) |
| 9 | good | health under 50 percent, solo or the healer far away | Rejuvenation (774, 4); Healing Touch (5185, 26) under 30 percent | yes: 28.0 and 28.1 (`CD:184-187`); **both leave Moonkin Form first** (`CasterDruidStrategy.cpp:135` adds the shapeshift nodes) |
| 10 | good | being attacked (a melee mob on the bot) | Barkskin (22812, 44), Nature's Grasp (16689, 52), flee from melee (derived: a caster should not trade blows) | yes from 44 and 52 (`GD:114-115, 139-140, 145-146`; flee `CD:198-199`) |
| 11 | expert | bar at 0 or just after a pull, solo | Astral Communion (127663, 20) out of combat to arrive in Eclipse (Method opener, wowtbc opener) | missing; low value because the bar is kept between fights (see Eclipse) |
| 12 | expert | the DoT has 3 s or less left and an Eclipse of its school is up | refresh it (Method: Sunfire in Solar Eclipse, Moonfire in Lunar; the Eclipse bonus is snapshotted, spell text of 79577) | missing: refresh window 0 |
| 13 | expert | moving | Moonfire or Sunfire (the one matching the Eclipse), then Starsurge only on a Shooting Stars proc (wowtbc: DoTs and Wild Mushroom while moving) | missing: the DoTs fire only when the aura is gone; the fillers are refused |

Before the spells are learned (derived): at 1 to 9 Wrath and Moonfire only; at 10 Starfire and the Eclipse loop start; at 12
Starsurge; at 16 Moonkin Form; at 18 Sunfire. The default list falls through in that order already (an unknown spell is skipped).

### Levels 30 to 44

As above, plus the tier 2 talent (30), Nature's Swiftness (30), Euphoria (38), Hurricane (42), Barkskin (44).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 14 | good | a target with armor-reliant damage dealers in the group, fight over 20 s | **Faerie Fire** (770, 28): Weakened Armor x3 for 30 s, skipped when another source already holds three stacks (derived: 7.5 percent mana and a global per refresh; neither guide lists it for Balance) | yes, but at **29.5, above every damage line** (`CD:167`, `DT.cpp:131-135`), on every target at the pull, with no check for who gains from it |
| 15 | good | 5 or more attackers within 8 yd, and mana above 60 percent | Hurricane (16914, 42) (wowtbc: 5 or more targets; Method: in Solar Eclipse) (derived: 50.3 percent of base mana per 10 s channel) | **wrong threshold and order:** "medium aoe" is 3 attackers within 8 yd (`GD:293-294`, 23), above the DoTs (18, 17.5) and Starsurge (17); no mana test |
| 16 | good | 2 or 3 attackers | the single target list with Moonfire and Sunfire on every attacker (wowtbc) | yes: `moonfire on attacker` 5.1 and `insect swarm on attacker` 5.2 (`GD:297-301`), below every other edge |
| 17 | good | Nature's Swiftness off cooldown | **do not spend it on a filler** (derived: the next cast becomes instant, which saves 2 s on one Wrath; it is better kept for a Healing Touch on a hurt ally) | cast on every cooldown at 29 (`GD:178-180`, `BuffTrigger`, so not behind the boost gate); harmless but wasteful |
| 18 | good | health 65 to 85 percent | Barkskin (22812, 44) on cooldown (60 s) | yes: 40 (`GD:139-140`) and 27 (`GD:114-115`) |

### Levels 45 to 59

As above, plus the tier 3 talent (45), Owlkin Frenzy (48), Nature's Grasp (52), Innervate (54), Rebirth (56).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 19 | core | mana under 40 percent | Innervate (29166, 54) on self; on the healer when the healer is under 40 percent (derived: the Balance druid burns 9 to 15 percent of base mana a cast; with the Innervate glyph the caster also gets mana) | self only, 29 (`CD:196-197`); the healer version exists only for the cat (`CatDpsDruidStrategy.cpp:140-141`) |
| 20 | good | a party member is dead in combat | Rebirth (20484, 56) | yes: 29 (`GD:133-134`); leaves Moonkin to cast it, which it does not need to (flags above) |
| 21 | good | tier 3 talent | Faerie Swarm: the module casts Faerie Fire 770 by name (see Talents) | to check live |
| 22 | not recommended | an enemy within melee, Typhoon picked | **Typhoon** (derived: it knocks the tank's pack away from the tank; only with the talent) | yes at 40 (`CD:160-161`); idle with the premade (Faerie Swarm) |

### Levels 60 to 74

As above, plus the tier 4 talent (60), Mark of the Wild (62), Hibernate (66), Celestial Alignment (68), Might of Ursoc (72).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 23 | core | out of combat, the group lacks it | Mark of the Wild (1126, 62) | yes: 14 and on the party 13 (`NC:112-113, 187-189`); **leaves Moonkin first** (`NC:31-45`) and asks the unknown name from level 10 on |
| 24 | good | entering an Eclipse | **Incarnation: Chosen of Elune** (talent 106731, 60) if picked (Method: on entering an Eclipse, both guides) | **missing** (gap 8) |
| 25 | good | an Eclipse is about to end (Method), or a boss or an elite | **Celestial Alignment** (112071, 68) | cast at once on `boost` at 29.0 (`GD:194-195`): at the pull, when solo; never in a normal group (balance rule); not timed to an Eclipse |
| 26 | good | the tier 4 pick Force of Nature | Force of Nature (106737, 3 charges) | `boost` 29.0 (`GD:188-189`); same gate |
| 27 | not recommended | health under 25 percent | **Might of Ursoc** (106922, 72) forces Bear Form for 20 s (derived: a caster in Bear Form cannot cast, and the Balance nodes shift it out at once) | yes for every spec: 90 (`GD:120-121`); whether the +30 percent health survives the shift is to check live |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | tier 5 talent: situational | no line for any pick |
| 76 | Starfall (48505) "as often as possible" (Method), first line in wowtbc; Hurricane from 5 targets | Starfall: 28.5 on cooldown (`GD:288-289`, `DT.h:308-312`); correct; costs 32.6 percent of base mana per 90 s (the cooldown resets on a Lunar Eclipse) |
| 78 | Cyclone (crowd control only) | `cc` strategy, needs a Cyclone mark |
| 80 | Mastery: Total Eclipse; stat order of Method | passive; stat weights (see Stat order) |
| 82 | Lunar Shower: Moonfire and Sunfire casts add stacking damage and cut cost; Method: Sunfire into Moonfire when entering Lunar, Moonfire into Sunfire when entering Solar | not used |
| 84 | Wild Mushroom x3 and Detonate in the next Solar Eclipse (wowtbc, moving filler) | no line |
| 90 | Method single target: DoTs, Starfall, Incarnation on entering an Eclipse, Celestial Alignment as one ends, Starsurge on cooldown and on a Shooting Stars proc, Wrath in Solar, Starfire in Lunar | everything but Incarnation and the timing of Celestial Alignment |

## Cooldowns and defensives

The guides say "on cooldown" and give Eclipse timings; the dungeon timing is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Incarnation: Chosen of Elune | talent 60 | right after entering an Eclipse | the same, on a boss, an elite or a pack of 3 or more | no line |
| Celestial Alignment | 68 | on cooldown, as an Eclipse ends | the same, on a boss or an elite | `boost` 29.0, at the pull, never in a normal group |
| Force of Nature | talent 60 | on cooldown in single target (wowtbc not listed) | a boss or an elite | `boost` 29.0, same gate |
| Starfall | 76 | as often as possible | on cooldown | yes |
| Nature's Vigil | talent 90 | not named (wowtbc: "easier" talent) | a boss or an elite | `boost` 28.9, same gate |
| Innervate | 54 | not named | the healer first, then self at 40 percent | self only |
| Solar Beam | 28 | not named | every interruptible cast of the pull target | none |
| Barkskin | 44 | defensive (wowtbc) | on cooldown when damaged, any form | yes |
| Might of Ursoc | 72 | defensive (both guides) | not for a caster (see line 27) | yes |
| Rebirth | 56 | not named | a dead group member in combat | yes |
| Nature's Grasp | 52 | not named | when a mob hits the druid | yes |
| Typhoon | talent 45 | not named | never in a dungeon | yes (if picked) |

## Healer triage and mana (not a healer)

Not applicable to triage. Resource: mana only. Per-cast costs in percent of base mana: Wrath 8.8, Moonfire 8.4, Sunfire 9.0, Starfire 15.5,
Starsurge 15.5, Faerie Fire 7.5, Solar Beam 16.8, Starfall 32.6, Hurricane 50.3, Healing Touch 28.9, Rejuvenation 14.5, Moonkin
Form 9.3, Cat Form 3.7, Mark of the Wild 5.0. Mana returns: every Eclipse (spell 81070, size to check), Innervate (54), the mana
potion, drinking (50 percent rule). Natural Insight (10) raises the pool. The bots: Innervate at 40 percent from 54, the generic
drink rule, the mana potion as Innervate's alternative (`GD:91-97`). Derived: a Balance druid below 54 has no mana cooldown at all, so
Starfire is the expensive filler: when mana is under 30 percent and a Wrath-Moonfire-Sunfire loop is enough, prefer Wrath (cost 8.8
against 15.5). The module has no such rule (the Eclipse direction rule always picks Starfire on the Solar side).

## Spell names the Balance strategies ask for that a Balance druid does not know

`SpellIdValue` logs `[SID] ... qualifier='<name>' resolved=0` whenever a name matches no known, non-passive spell
(`strategy/value/SpellIdValue.cpp:121-149`) and caches the answer 20 s per name and bot (`:24`). A trigger that tests the spell id
(`SpellNoCooldownTrigger`, `NaturesSwiftnessTrigger`) asks too, so a known-spell test alone does not stop the log; the level gate
has to be at registration or the log has to skip a spell the class gets later. From the code (inference, not checked against the
log), the names a level 25 Balance druid asks for and does not know, by source; the "learned at" column is for Balance:

| Name | Learned at | Asked from |
|------|-----------:|------------|
| `faerie fire` | 28 | `CD:167` (debuff trigger, every tick with a target) |
| `healing touch` | 26 | `CD:186-187` (health under 30 percent) |
| `solar beam` | 28 | not asked (no line) |
| `hurricane` | 42 | `GD:293-294` ("medium aoe") |
| `barkskin` | 44 | `GD:114-115` and `GD:139-140` (health 65 to 85 percent) |
| `nature's grasp` | 52 | `GD:145-146` (being attacked) |
| `nature's swiftness` | 30 | `GD:178-180`; `DT.cpp:101-105` asks the id every tick |
| `innervate` | 54 | `CD:196-197` (medium mana) |
| `rebirth` | 56 | `GD:133-134` |
| `mark of the wild` | 62 | `NC:112-113`, party edge `NC:187-189` |
| `might of ursoc` | 72 | `GD:120-121` (health under 25 percent) |
| `force of nature` | talent 60, and only that pick | `GD:188-189` |
| `celestial alignment` | 68 | `GD:194-195` |
| `nature's vigil` | talent 90, and only that pick | `GD:196-197` |
| `starfall` | 76 | `GD:288-289` (the trigger asks the id every tick) |
| `typhoon` | talent 45, and only that pick | `CD:160-161`, `GD:295-296` ("enemy within melee") |
| `moonkin form` | 16 | prerequisite of every spell node (`CD:12-130`): asked at levels 10 to 15 |
| `sunfire` | 18 | `CD:173-174`: asked below 18 |
| `starsurge` | 12 | `CD:180-181`: asked below 12; `starfire` below 10 (`CD:146`) |
| `cyclone`, `hibernate` | 78, 66 | `cc` strategy, only with a crowd control mark |

Fix direction: build the edge lists with a level gate (the bot's level against the spell's learn level) so the lists above shrink to
the spells the druid owns, and keep the talent-only names behind a "has the talent" test.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Starfire, Eclipse and Balance of Power arrive at 10, Starsurge at 12, Moonkin Form at 16, Sunfire at 18,
  Astral Communion at 20, Shooting Stars at 26, Solar Beam and Faerie Fire at 28, Euphoria at 38, Hurricane at 42, Innervate at 54,
  Mark of the Wild at 62, Celestial Alignment at 68, Starfall at 76, Mastery: Total Eclipse at 80, Wild Mushroom at 84; below those
  levels the lists above start without them.
- The Eclipse bar moves only with the matching cast (core script); the guides' two-spell rule is a direction rule here.
- Astral Communion gives 25 energy per second for 4 s in the bar's direction; the bar survives combat, so the guides' opener
  (Astral Communion to 50 Lunar Energy) saves only the first pull of a session.
- Healing Touch before an Eclipse (wowtbc) is a Dream of Cenarius line (level 90), not a general rule.
- Method's "Haste at 5273, then 8089" are level 90 breakpoints.
- Mark of the Wild is level 62 here (it was level 1 in older versions).
- Challenging Roar, Insect Swarm and Dire Bear Form do not exist (BT-146 and the older BT-086); "insect swarm" in the module is Sunfire.
- Overgear, Icy Veins and Wowhead were not readable, so two guides were compared.

## Gap list (ordered)

Config (can go at once):

1. Talent link `AiPlayerbot.PremadeSpecLink.11.0` (`conf/playerbots.conf.dist:2926`) from `100112` to **`000210`** (Feline Swiftness, Ysera's
   Gift, Faerie Swarm, Force of Nature, Ursol's Vortex, Heart of the Wild) until the Incarnation line of gap 8 exists, then **`000110`**
   (Method's Incarnation). Reasons: Displacer Beast has no line, Incarnation has no line, Heart of the Wild is a passive that
   Method calls the best DPS pick while Nature's Vigil sits behind a gate that a normal group rarely opens. The comment at `:2921-2924` needs
   the same change.
2. Glyph order `PremadeSpecGlyph.11.0` (`:2927`) to `40914,43332,40908,104102,43316,40909` (Healing Touch, Grace, Innervate, Sprouting
   Mushroom, Aquatic Form, Rebirth): Rebirth at 25 and Stampede at 50 do nothing until 56 and 84.
3. Stat weights for Balance, split from the Shadow priest (`Mgr/Item/StatsWeightCalculator.cpp:535-546`): Intellect 1.5, Spirit 1.1 (equal
   to Hit until the hit cap, Balance of Power), Mastery 0 below level 80, keep Hit 1.1, Haste 1.0, Critical 0.8; the hit cap check
   (`:903-940`) should add the Spirit from gear for a Balance druid.

Strategy (class fixes, wait for the owner's in-game test), ranked by value for levels 15 to 60:

4. **Unknown-name spam** (the level 25 table above): level gates on the edges of `CasterDruidStrategy`, `GenericDruidStrategy` (boost,
   barkskin, nature's grasp, rebirth, might of ursoc, aoe), the non-combat Mark of the Wild edges and the moonkin prerequisites;
   talent-only names behind a talent test. Verify with the debug log: the `resolved=0` lines per bot fall to near zero.
5. **Do not leave Moonkin Form to heal, buff or resurrect** (data: Moonkin Form counts as not shifted for those spells):
   `CastCasterFormAction::isUseful` (`DruidShapeshiftActions.cpp:35-41`) returns false for Moonkin and Tree of Life when the next spell is
   Healing Touch, Rejuvenation, Regrowth, Mark of the Wild, Revive, Rebirth or Remove Corruption; or remove the "caster form" prerequisite
   from those nodes for the Balance (`FeralDruidStrategy.h:12-70` added at `CasterDruidStrategy.cpp:135`, `NC:31-80`,
   `GD:83-89`, `DruidActions.cpp:87-97`). Saves 9.3 percent mana and a global at each use. Check live that the casts go through in the form.
6. **Moonkin Form out of combat**: a non-combat edge from 16 (the pattern of `TankBearFormTrigger`, `DT.cpp:475-501`, silent for eating,
   drinking, travel forms, mounts and resurrection), so the first cast of a pull is Starfire, not a shift.
7. **Solar Beam** (28): an `InterruptSpellTrigger` on "solar beam" at the interrupt rank (the shape of `BashInterruptSpellTrigger`,
   `DT.h:154-158`) and a `CastSpellAction`; the spell takes a unit target (effect target type 6, SpellEffect data) and has a 40 yd range.
8. **Incarnation: Chosen of Elune** line (needed only if the tier 4 pick is Incarnation): cast when Eclipse (Solar) 48517 or Eclipse
   (Lunar) 48518 has just appeared and the fight is a boss, an elite or a pack of 3 or more; the Balance Overrides Passive turns the
   talent into 102560 in `BotOverrideSpell`.
9. **Innervate on the healer**: move the "healer low mana" edge (`CatDpsDruidStrategy.cpp:140-141`) into the Balance strategy at 35, keep
   the self edge at 40 percent as the second choice; the Innervate glyph then returns mana to the caster.
10. **Boost gate for the Balance cooldowns** (`GD:186-198`): replace the "balance 50 or less" rule by "the pull target is an elite or a boss,
    or 3 or more attackers" for Force of Nature, Celestial Alignment and Nature's Vigil; time Celestial Alignment to the end of an Eclipse
    (the expert line).
11. **Hurricane**: raise the trigger to 5 attackers within 8 yd, below the DoTs, with a mana test (50.3 percent of base mana): `GD:293-294`;
    keep the channel cancel (`DT.cpp:307-343`) and add the Astral Storm ids to the set (`:299-305`).
12. **Faerie Fire**: out of the 29.5 slot to just under the DoTs and only on a boss or elite with a melee damage dealer or the tank in the
    group (`CD:167`).
13. **Moving**: an instant fallback (Moonfire or Sunfire of the Eclipse school) when a cast-time spell is refused because the bot moves;
    let Starsurge pass the moving test when Shooting Stars is up (`AI/PlayerbotAI.cpp:3849-3850`, `:3921-3922`).
14. **Might of Ursoc for casters**: take the Balance out of the generic edge (`GD:120-121`) unless a live check shows that the health
    bonus outlasts the forced form.
15. **Nature's Swiftness**: leave it for a Healing Touch on a hurt ally (the resto pattern, `GD:176-180`) instead of casting it at 29 on
    every cooldown.
16. **DoT refresh window** (expert): a refresh time of one Starfire (about 3 s) on the Moonfire and Sunfire triggers (`DT.h:111-115`,
    `DebuffTrigger` `beforeDuration`), matching Method's "refresh in Eclipse".
17. Skill tiers: no gating exists in the Balance strategies (`BotSkill` is used in the restoration triggers only). Lines marked good and
    expert above (Solar Beam, the DoT refresh, the moving fallback, Faerie Fire, the cooldown timing) are the ones a casual bot drops.
18. To check live: Faerie Fire 770 after Faerie Swarm; Moonkin casting Healing Touch, Rejuvenation and Mark of the Wild without
    leaving the form; the Astral Storm swap in Lunar Eclipse; Might of Ursoc and the Moonkin nodes; the "flee" edge at 39 near the tank;
    the real mana cost of a pull at levels 20 to 50; whether Typhoon 61391 (class line, acquire method 0) is ever known without the
    talent.

Owner questions:

1. Tier 4 (level 60): Incarnation (Method's raid default; needs gap 8), or Force of Nature (a line exists; treants also cast
   Entangling Roots on the target), or Soul of the Forest (idle).
2. Tier 6 (level 90): Heart of the Wild (Method's "always best", passive) or Nature's Vigil (current; a line, behind the boost gate).
3. Tier 1: Feline Swiftness (passive speed, derived) or Wild Charge (moonkin backward bound, no line) or Displacer Beast (current, idle).
4. Glyphs: the derived order that works at each unlock level, or the guides' Rebirth and Stampede (utility, idle for 30 to 60 levels).
5. Hurricane at 5 attackers (wowtbc) with a mana test, or the current 3 attackers.
6. Faerie Fire: boss and elite fights only, or on every pull as today.
7. Should the Balance druid hand Innervate to the healer before using it itself (derived; the guides are silent).

## PvP play (player guides, 5.x, read 2026-11-19)

- **Sources:** two player guides for Balance in arenas: one for 5.4 by a 2600 rated player (talents, glyphs, stats), one long guide from the MoP Classic client (Eclipse play, opener, utility, comps; its numbers are Classic, not 5.4.8, and are flagged). No coach course. Facts only, nothing copied; data checked against this server's 5.4.8 tables.
- **Coverage:** talents, glyphs, the Eclipse rotation and the opener are covered; matchups, interrupts, positioning and defensive timing beyond the tier picks are GAP (marked "adapted" where we borrow from the Restoration plan's control kit).

**The plan.** Balance plays like a damage-dealing healer: kite, heal while running, go in for burst, come out. In a double damage 2v2 it is the pseudo healer. Good partners named: rogue, warlock, mage in 2v2; warlock-based and mage plus holy paladin in 3v3.

**Rotation.** Stay in Eclipse as much as possible: Eclipse raises damage, gives about 15 percent spell haste for 15 s (Nature's Grace) and mana. In an Eclipse refresh the dot of its school (Moonfire in Lunar, Sunfire in Solar), cast the matching direct spell (Starfire in Lunar, Wrath in Solar), and spend Shooting Stars procs inside it, not before. Use Astral Communion in downtime to swap the Eclipse. Starsurge also moves the bar, so it competes with holding an Eclipse. In 5.4 a Shooting Stars proc is no longer farmed by spreading dots on many targets (the chance falls with each extra target), so dot one or two targets and cast.

**Opener (expert).** Before the gates: Cenarion Ward on self, Astral Communion to one tick before Lunar Eclipse, cancel, Prowl. Out of stealth: Starfall, finish the Communion into Lunar Eclipse, dot; Lunar Eclipse resets Starfall, so cast it again, then Celestial Alignment (which also resets it) and a third Starfall. Refresh both dots just before Celestial Alignment ends. Pounce from stealth stuns 4 s.

**Control and defensives.** Typhoon is locked in. Force of Nature treants cast Entangling Roots off the global cooldown. Tier 75 by comp: Mighty Bash with casters, Disorienting Roar with a rogue or a team that already stuns (it shares diminishing returns with Dragon's Breath and Scatter Shot, not with Polymorph or Trap), Ursol's Vortex with Incarnation. Nature's Swiftness makes the next Cyclone, Hibernate or Entangling Roots instant; against a druid cast Hibernate that way, then Cyclone. Faerie Fire silence in Bear Form (glyph, 15 s cooldown) is an extra range kick; Might of Ursoc puts you in Bear Form off the global cooldown. Cenarion Ward is the main heal on self and partner. Displacer Beast is kept until the enemy used a gap closer; Wild Charge can pull the druid to a trapped healer so the trap lands on the druid.

**Talents and glyphs, guides against the set** (`PremadeSpecLinkPvp.11.0 = 122220`, glyphs `45622,43332,67484,44922,89868,45623`):

| Tier | Guides | Set | Ladder (8 pooled) |
|---|---|---|---|
| 15 | Displacer Beast (Wild Charge niche) | Displacer Beast | 8 of 8 |
| 30 | Cenarion Ward | Cenarion Ward | 8 of 8 |
| 45 | Typhoon | Typhoon | 8 of 8 |
| 60 | Force of Nature (Incarnation for burst; Soul of the Forest useless) | Force of Nature | 8 of 8 |
| 75 | Mighty Bash, Disorienting Roar or Vortex by comp | Mighty Bash | 5 of 8 Bash |
| 90 | Heart of the Wild (best default) or Dream of Cenarius (the 5.4 guide) | Heart of the Wild | 7 of 8 |

Glyphs the guides want: Cyclone (range), Barkskin (fewer crits), Guided Stars (not in the set; keeps Starfall from breaking a sheep and focuses it), and by comp Fae Silence, Might of Ursoc. The set matches except Guided Stars. Stat order (steers rolls only): hit 6 percent (Spirit counts), then either all crit or haste to a breakpoint and then crit; the 5.4 guide plays haste first.

**Differences from the guides (5.4.8).** Wild Charge flies to a nearby ally (data text), so jumping to a Wild Mushroom is a Classic claim (Wild Mushroom is learned at level 84). The double combo points from Primal Fury and a 5 point Maim in cat form are Classic claims: Maim is learned at level 82 and the Balance list has no Primal Fury. Typhoon's "30 yards" is Classic. Heart of the Wild no longer makes the druid immortal for 45 s (the 5.4 guide's point). Starfall resets on a Lunar Eclipse (data text agrees). Level gates: Starsurge 12, Moonkin Form 16, Sunfire 18, Astral Communion 20, Shooting Stars 26, Solar Beam 28, Nature's Swiftness 30, Celestial Alignment 68, Might of Ursoc 72, Starfall 76, Wild Mushroom 84.

**Bots today (read from `src/strategy/Classes/druid/`, ordered by value).**
1. No crowd control chain: Cyclone is the dungeon mark trigger only (`GenericDruidStrategy.cpp:255`); Nature's Swiftness is a heal for Balance (`CasterDruidStrategy.cpp:158-166`); Mighty Bash and Disorienting Roar are never cast (the tier 75 pick is idle).
2. No Astral Communion, no Starfall opener: Starfall is on the area strategy only and Celestial Alignment is a plain cooldown at 29.0 (`GenericDruidStrategy.cpp:117`). Shooting Stars procs are not held for an Eclipse; Starsurge is a plain 17.0 edge (`CasterDruidStrategy.cpp:204`).
3. Cenarion Ward, Displacer Beast and Wild Charge have no PvP rule.
4. Barkskin at low health only (`GenericDruidStrategy.cpp:131,155-156`); Might of Ursoc is skipped for Balance (`:135-139`).
5. What exists: Solar Beam on casters from level 28 (`CasterDruidStrategy.cpp:172`), Typhoon on a melee enemy (`:179-181`), Force of Nature on cooldown (`GenericDruidStrategy.cpp:115`), Eclipse direction edges (`CasterDruidStrategy.cpp:212-219`), dots and Starsurge (`:195-204`), Nature's Vigil and Celestial Alignment (`GenericDruidStrategy.cpp:116-117`). Barkskin and Survival Instincts are in the shared defensive catalog.

**Research update (public patch notes 5.1 to 5.4.7 and spell data, 2026-11-19; lines marked derived come from game knowledge and the 5.4.8 data).** Starfall stopped striking crowd controlled targets in 5.2, so the opener never breaks a sheep and Glyph of Guided Stars is a focus tool; Force of Nature costs no global cooldown and holds up to three charges, one per 20 s (5.3); Solar Beam silences up to 8 s in its area (5.1); Typhoon and other area knockbacks share one category with 10 s immunity (5.4.0); Faerie Fire lasts 20 s in PvP (5.4.0); Heart of the Wild turns 100 percent of PvP Power into healing for Balance (5.4.7). Data: Cyclone 1.7 s cast, 20 yd (25 with the glyph), 6 s; Mighty Bash 50 s, 5 yd, 5 s (it needs a closing tool: Displacer Beast or Wild Charge); Solar Beam 60 s. Derived: Cyclone is its own diminishing returns category, so it is the safe first and last link of a chain; Disorienting Roar shares the disorient category (not sheep or trap); Bash shares the stun category with the partner's stuns; open with Prowl at a pillar beside the healer, hold Barkskin and Might of Ursoc; the go is a partner's control on the healer plus a Solar Beam ready for its cast. Extra gap lines: Starfall is area-strategy only (`CasterDruidStrategy.cpp:142-143`); Typhoon is cast on any melee enemy without the knockback immunity check (`:179-181`); Nature's Swiftness never serves a Cyclone for Balance (`:158-166`).

**Proposals.** Config (no code): the set already matches the guides; add Glyph of Guided Stars only with a mage partner; tier 90 stays Heart of the Wild (guides and ladder agree). Rotation (waits for the owner's in-game test): Astral Communion plus triple Starfall opener; hold Shooting Stars for an Eclipse; a control chain (Roar or Bash, then Cyclone on the healer, Nature's Swiftness Cyclone) adapted from the Restoration plan; Cenarion Ward on self at the start; Displacer Beast after the enemy's gap closer.

**Open questions.** (1) Heart of the Wild or Dream of Cenarius at tier 90 (the 5.4 guide and the Classic guide disagree). (2) Whether the mushroom jump works on this core. (3) Whether a Balance bot should enter cat form at all.

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 4 players of this specialisation (rating 2474 to 2549). 2v2: 4 players (rating 2656 to 2727). Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** pooled (8 players)
- **BT-324 result:** updated (BT-453, pooled 8 players)
- **Set now:** `PremadeSpecLinkPvp.11.0 = 122220` (was `122000`), `PremadeSpecGlyphPvp.11.0 = 45622,43332,67484,44922,89868,45623` (was `45622,43332,67484,104102,43316,45623`)

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Displacer Beast 100% / 100% / 8 of 8 | Displacer Beast | Displacer Beast | matches |
| 2 (30) | Cenarion Ward 100% / 100% / 8 of 8 | Cenarion Ward | Cenarion Ward | matches |
| 3 (45) | Typhoon 100% / 100% / 8 of 8 | Typhoon | Typhoon | matches |
| 4 (60) | Force of Nature 100% / 100% / 8 of 8 | Soul of the Forest | Force of Nature | changed (8 of 8) |
| 5 (75) | Mighty Bash 50% / 75% / 5 of 8, Disorienting Roar 50% / 25% / 3 of 8 | Disorienting Roar | Mighty Bash | changed (5 of 8) |
| 6 (90) | Heart of the Wild 75% / 100% / 7 of 8, Nature's Vigil 25% / 0% / 1 of 8 | Heart of the Wild | Heart of the Wild | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Cyclone (45622) | 100% | 100% | 8 of 8 | yes | yes |
| major | Barkskin (45623) | 100% | 100% | 8 of 8 | yes | yes |
| major | Fae Silence (67484) | 75% | 25% | 4 of 8 | yes | yes |
| major | Might of Ursoc (45603) | 25% | 25% | 2 of 8 | no | no |
| minor | Stars (44922) | 100% | 75% | 7 of 8 | no | yes |
| minor | the Cheetah (89868) | 100% | 75% | 7 of 8 | no | yes |
| minor | Grace (43332) | 75% | 50% | 5 of 8 | yes | yes |
| minor | Aquatic Form (43316) | 25% | 50% | 3 of 8 | yes | no |
| minor | the Sprouting Mushroom (104102) | 0% | 0% | 0 of 8 | yes | no |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **What changed (pooled 4 + 4 = 8):** tier 60 is Force of Nature (8 of 8; the balance rotation casts it at `GenericDruidStrategy.cpp:216-220`) instead of Soul of the Forest; tier 75 is Mighty Bash (5 of 8, 62 percent: 3v3 split 2 of 4, 2v2 3 of 4) instead of Disorienting Roar (3 of 8); the minor glyphs Stars and the Cheetah (7 of 8 each, both cosmetic form looks) replace the Sprouting Mushroom (0) and Aquatic Form (3 of 8). Heart of the Wild 7 of 8, Cyclone and Barkskin 8 of 8, Fae Silence 4 of 8 and Grace 5 of 8 stay.
- **Open (BT-455):** no bot rule casts Mighty Bash (grep of `src`, 2026-11-18); until one exists the tier is idle in a match. Disorienting Roar had a PvP and solo rule (BT-322). The tier 75 change rests on 5 of 8 players, the thinnest margin above 60 percent.
