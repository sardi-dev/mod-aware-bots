# Shaman: Elemental (damage, ranged caster)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). This pass added no
  rotation change. The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Elemental Shaman Class Guide, MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 23 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the names in the text are usable).
  - wowtbc.gg, "Elemental Shaman" MoP Classic PvE guide (no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellInterrupts, SpellCategories, ChrClasses; `data/db2/Item.db2` and
    `Item-sparse.db2` for glyph item ids) through `spec_data.py` and short read-only scripts; the core source (the SkyFire core: `scripts/Spells/spell_shaman.cpp`, `Spell.cpp`) for the spell scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403, as noted in the skill), Wowhead (needs a browser).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's
  marks), 61 to 90 briefly.

Which source was richer: **picks per tier, the stat order, the cooldown list** Method; **the option list of all 18 talents, glyph
thresholds (4+ and 6+ targets), numeric area rules (2, 3 to 5, 6+), the Ascendance condition, the opener with a pre-pot, the four
named defensives, the totem list** wowtbc. Neither guide has levelling advice, a mana plan, a threat rule or a word on what an
Elemental shaman does before Lava Burst (34), Chain Lightning (28) or Flame Shock (12), so those lines come from the 5.4.8 data
and are marked "derived". The guides disagree or are wrong on five points:

- **Opener pre-cast.** Method: Lightning Bolt 2.5 s before the pull; wowtbc: Fire Elemental Totem, then Lava Burst. Pick
  Method's Lightning Bolt (derived: Lava Burst's extra damage needs your Flame Shock on the target, and a pre-cast Lava Burst
  has none; the Fire Elemental is a level 66 spell).
- **Ascendance condition.** Method: "with no Lava Burst charge"; wowtbc: Flame Shock 15 s or more left, Lava Burst on
  cooldown and Unleash Elements active. Pick wowtbc (more exact; level 87 only).
- **Earth Elemental Totem.** wowtbc lists it (when Fire Elemental is not out); Method does not. Not picked (derived: it is a
  protective 5 minute cooldown, not a damage button).
- **Glyph list.** Method names the glyph of Spirit Walk for an Elemental shaman: logic error, Spirit Walk is an Enhancement
  spell (SpecializationSpells spec 263, level 60; spec 262 does not have it). wowtbc's list is the one used.
- **Lightning Shield charges.** Method "above 6", wowtbc "6 to 7": same rule. In this core (`spell_sha_fulmination`) Earth
  Shock consumes every charge above one, so waiting for 6 is a damage choice, not a rule (see Talents notes).

## Talents

Tiers and columns are from Talent.dbc (the three shaman specialisations share one talent list). "Bots today" is
`AiPlayerbot.PremadeSpecLink.7.0 = 200010` (`conf/playerbots.conf.dist:2819`; one digit per tier, column counted from 0):
Astral Shift, Frozen Power, Call of the Elements, Elemental Mastery, Ancestral Guidance, Unleashed Fury.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (200010) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Astral Shift (Method defensive; wowtbc lists all three) | Nature's Guardian (passive, bot-friendly); Stone Bulwark Totem (a shield totem, wired for this spec) | 108271 / 30884 / 108270 | Astral Shift, same. **Inert: no line casts it** (grep of `src/` finds none). The Elemental strategy asks for Stone Bulwark at low health (`CasterShamanStrategy.cpp:59-60`), which the premade does not take |
| 2 (30) | Windwalk Totem (Method default) | Earthgrab Totem when a mass root is needed; Frozen Power | 108273 / 51485 / 63374 | **Frozen Power, differs and inert:** it makes Frost Shock root, and no strategy casts Frost Shock (`FrostShockSnareTrigger` is registered, `ShamanTriggers.h:46`, and used by no edge). Windwalk is inert as well; the change only aligns the premade with the guide |
| 3 (45) | Call of the Elements (Method default) | Totemic Projection when moving much (Method: preferred in dungeons); Totemic Persistence (wowtbc lists it) | 108285 / 108287 / 108284 | Call of the Elements, same. Inert: the edge was removed because the old name meant a different spell (`CasterShamanStrategy.cpp:52-58`) |
| 4 (60) | Elemental Mastery or Echo of the Elements, near-equal (Method) | Ancestral Swiftness (wowtbc lists it) | 16166 / 108283 / 16188 | Elemental Mastery, same: a cast edge at 29 (`CasterShamanStrategy.cpp:94-95`, trigger `ShamanTriggers.cpp:299`) |
| 5 (75) | Ancestral Guidance (Method: recommended) | Conductivity and Rushing Streams are healer picks | 108281 | Ancestral Guidance, same: cast at low health, 38 (`:66-67`) |
| 6 (90) | **Elemental Blast** (Method, single target) | Primal Elementalist for AoE and dungeons (Method); Unleashed Fury (wowtbc lists it) | 117014 / 117013 / 117012 | **Unleashed Fury, differs:** the default list asks for Elemental Blast on every tick (`:42`), which a shaman with Unleashed Fury never knows |

Notes on the picks (data, not guide):

- Astral Shift (108271): damage taken -40 percent for 6 s, 90 s cooldown, no cost, usable while stunned. Nature's Guardian
  (30884): below 30 percent health, maximum health +25 percent for the next 6 s (effect data) and lower threat, 30 s cooldown;
  it works without any code. Stone Bulwark Totem (108270): 8.0 percent of base mana, 30 s totem, an absorb shield on the shaman.
- Call of the Elements (108285) finishes the cooldown of every totem with a base cooldown under 3 minutes (3 minute cooldown).
  Nothing in the bots asks for it; no loss.
- Elemental Mastery (16166): +30 percent haste for 20 s, 90 s cooldown (effect data).
- **Fulmination (88766, level 20) in this core:** `spell_sha_fulmination` (`spell_shaman.cpp`, in the core) runs after Earth
  Shock, needs at least 2 Lightning Shield charges, deals (charges - 1) Lightning Shield orbs and leaves 1 charge (the spell
  text speaks of charges above a threshold; the effect's base point is 1). So Earth Shock with the shield up is a real
  damage button here from 2 charges; the guides' "wait for 6 to 7" maximises damage per Earth Shock and is the expert rule.
  Rolling Thunder (88764, level 20): a Lightning Bolt or Chain Lightning has a chance to add a charge (up to 7) and give back
  2 percent of mana (effect data).
- Elemental Precision (30674, level 10): hit equal to the Spirit on the shaman's items (spell text).

Proposed link (derived from the table): **`220012`** = Astral Shift (needs gap 7), Windwalk Totem, Call of the Elements, Elemental
Mastery, Ancestral Guidance, Elemental Blast. Only the tier 30 and tier 90 digits change. Owner questions 1 and 2.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`, effect text from Spell.dbc. Slots unlock in
pairs at 25, 50 and 75 (config comment `conf/playerbots.conf.dist:2631-2636`). The config order is major, minor, major, minor,
minor, major; a glyph goes to the first free slot of its type. "Bots today" is `AiPlayerbot.PremadeSpecGlyph.7.0 =
41531,44923,71155,43381,43385,41539` (`conf.dist:2820`).

| Type | Glyph | Item id | Why (one clause) | Bots today |
|------|-------|--------:|------------------|------------|
| Major | Flame Shock | 41531 | wowtbc standard, Method optional; Flame Shock damage heals the shaman (spell 55447) and the bots keep it up | yes, slot 1 (level 25) |
| Major | Lightning Shield | 71155 | both guides (Method: required); after the shield triggers, 10 percent less damage taken for 6 s (142912) | yes, slot 3 (level 50). **Dead while the Elemental never casts Lightning Shield** (gap 8) |
| Major | Spiritwalker's Grace | 41539 | wowtbc standard; +duration | yes, slot 6 (level 75). **Dead:** the spell is level 85 and no line casts it |
| Major, proposed for slot 6 | Chain Lightning | 41518 | both guides (4+ targets); two more jump targets, 10 percent less damage (55449) | no |
| Major, alternative | Thunder | 45770 | both guides (6+ targets); Thunderstorm cooldown 45 s to 35 s, and Thunderstorm is the bot's mana tool | no |
| Major, alternative | Totemic Recall | 41535 | not in a guide (derived); the recall returns 100 percent of the totem cost instead of 25 | no |
| Major, raid only | Healing Stream Totem, Fire Elemental Totem | 41533, 41529 | guides: raid cooldowns; not for a bot that drops no such totem | no |
| Minor | Thunderstorm | 44923 | wowtbc; removes the 20 yard knockback, which matters for a pack standing on the tank | yes, slot 2 (level 25) |
| Minor | Astral Recall, Far Sight | 43381, 43385 | cosmetic (shorter cooldown, usable indoors) | yes, slots 4 and 5 |

Disagreement: Method puts Lightning Shield first, Flame Shock optional, Chain Lightning and Thunder as AoE picks; wowtbc has
Flame Shock, Lightning Shield, Spiritwalker's Grace as standard. The premade follows wowtbc. Proposed
`PremadeSpecGlyph.7.0` (derived: a glyph in a slot must do something at the level the slot unlocks): **`41531,44923,71155,43381,43385,41518`**
(Chain Lightning in the level 75 slot instead of Spiritwalker's Grace). If the owner keeps Water Shield (question 3), the
Lightning Shield glyph becomes Chain Lightning and the third major Thunder. Owner question 3.

## Stat order

Method: Intellect, then enough Hit (Spirit and Expertise are named in the same line) for 15 percent spell hit, then Mastery, then
Haste, then Critical Strike; the author's advice is solid Haste first, then Mastery. wowtbc has no stat order. The caps are level
90 numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Intellect first** (the primary stat; the shaman's mana pool and spell crit).
- **Spirit counts as Hit**: Elemental Precision (30674, level 10) grants hit equal to the Spirit on items. Until the hit cap,
  Spirit is worth as much as Hit; after it, nothing (Elemental has no Meditation, which is Restoration only).
- Then Haste (Shamanism cuts the Lightning Bolt and Chain Lightning cast time to 2.0 s and 1.5 s; the cooldown on Lava Burst
  stays 8 s), then Critical Strike (Elemental Focus, level 40, comes from non-periodic crits).
- **Mastery last below level 80**: Mastery: Elemental Overload (77222) is learned at 80 (SpecializationSpells), so before it a
  Mastery rating does nothing. From 80 Method ranks it above Haste.
- Mail from level 40 (the Mail skill, 8737) and Mail Specialization at 50 (a bonus for mail with the right role), so mail over
  leather from 40 and cloth never.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:560-569`): Intellect 0.5, Spell Power 1.2, Hit 1.1, Critical 0.8, Haste 1.0,
  **Mastery 1.3 at every level** (the highest secondary, and dead below 80), mana regeneration 0.5, **no Spirit weight** (a
  Spirit item is worth nothing although it converts to hit). Proposed (derived): Intellect 1.5, Spell Power 1.0, Spirit 1.1
  (equal to Hit until the cap), Hit 1.1, Haste 1.0, Critical 0.8, Mastery 0 below 80 and 1.2 from 80; the hit cap check should
  add Spirit (the shadow priest and the warlock sheets have the same fix).

## What the specialisation knows, by level (5.4.8 data)

An Elemental shaman knows the Shaman class line (SkillLineAbility 924, classMask shaman) plus the Elemental rows of
SpecializationSpells (spec 262), less the rows that belong to another specialisation's list only. Ids are the spells the player
casts. Costs are percent of base mana (SpellPower); the global cooldown is 1.5 s. Shock spells share one 6 s cooldown
(category 19). The specialisation swap list for Elemental is empty (no spell replaces another), so the specialisation part of
`BotOverrideSpell` changes nothing; the aura swaps still work (Ascendance turns Chain Lightning into Lava Beam). **Chain Heal (44) and Healing Rain (60) are in the class line without a specialisation row** (to check live that
a non-Restoration shaman really has them).

| Level | Spell (id) |
|------:|------------|
| 1 | Lightning Bolt (403, 7.1 percent, 2.5 s; with Shamanism 62099 at 10: +70 percent damage and 2.0 s; the spell has no movement interrupt flag, see "Casting while moving") |
| 6 | Earth Shock (8042, 14.4 percent, instant, 6 s shock cooldown, 5 s with Spiritual Insight) |
| 7 | Healing Surge (8004, 34.3 percent, 1.5 s) |
| 8 | Lightning Shield (324, no cost, 60 min; 1 charge in the data, an orb hit on attackers; Rolling Thunder raises it to 7) |
| 10 | Flametongue Weapon (8024, 7.0 percent); **Thunderstorm** (51490, 45 s cooldown, damages and knocks back enemies 20 yd, restores mana about 15 percent); Shamanism, Elemental Fury, Elemental Precision, Elemental Reach, Ancestral Focus, Spiritual Insight (passives) |
| 12 | **Flame Shock** (8050, 11.9 percent, instant, 30 s DoT); Purge (370, 16.4 percent) |
| 14 | Ancestral Spirit (2008) |
| 15 | Ghost Wolf (2645, 7.2 percent, instant, +30 percent speed); tier 1 talent |
| 16 | Searing Totem (3599, 5.9 percent, 60 s); **Wind Shear** (57994, 9.4 percent, 12 s cooldown, 3 s lockout) |
| 18 | Cleanse Spirit (51886, curses only) |
| 20 | Water Shield (52127); **Rolling Thunder** (88764) and **Fulmination** (88766), both need Lightning Shield |
| 22 | Frost Shock (8056, 21.1 percent, 50 percent slow) |
| 24 | Water Walking (546) |
| 26 | Earthbind Totem (2484, 5.9 percent) |
| 28 | **Chain Lightning** (421, **30.5 percent**, 2.0 s, 3 s category cooldown, removed by Shamanism, so 1.5 s and no cooldown) |
| 30 | Healing Stream Totem (5394, **23.5 percent**, 30 s cooldown, 15 s); Totemic Recall (36936, returns 25 percent of the cost of each recalled totem); tier 2 talent |
| 32 | Reincarnation (20608) |
| 34 | **Lava Burst** (51505, 7.7 percent, 2.0 s, 8 s cooldown, always crits, extra damage with your Flame Shock on the target); Astral Recall |
| 36 | Magma Totem (8190, 21.1 percent, 60 s); Far Sight |
| 38 | Grounding Totem (8177) |
| 40 | Elemental Focus (16164, Clearcasting after a non-periodic crit); Burning Wrath (77747, raid spell power); Mail |
| 44 | Chain Heal (1064, class line) |
| 45 | tier 3 talent |
| 46 | Frostbrand Weapon (8033) |
| 50 | **Lava Surge** (77756: Flame Shock ticks can reset Lava Burst and make the next one instant); Mail Specialization |
| 54 | Tremor Totem (8143) |
| 55 | Elemental Oath (51470, +5 percent spell haste for the group) |
| 58 | Earth Elemental Totem (2062, 28.1 percent, 5 min cooldown) |
| 60 | **Earthquake** (61882, **70.3 percent**, 2.5 s cast, 10 s duration, 10 s cooldown, ground area); Healing Rain (73920, class line); tier 4 talent |
| 63 | Capacitor Totem (108269) |
| 65 | Shamanistic Rage (30823, 15 s, 60 s cooldown: -30 percent damage taken, spells cost nothing); Healing Tide Totem (108280) |
| 66 | Fire Elemental Totem (2894, 26.9 percent, 60 s, 5 min cooldown) |
| 70 | Bloodlust (2825), Heroism (32182); both rows are in the class line (faction filter not verified) |
| 72 | Bind Elemental (76780) |
| 75 | Hex (51514); Rockbiter Weapon (8017, no use); tier 5 talent |
| 78 | Stormlash Totem (120668, 5.9 percent, 10 s, 5 min cooldown) |
| 80 | Mastery: Elemental Overload (77222); Grace of Air (116956) |
| 81 | Unleash Elements (73680, 8.2 percent, 15 s cooldown) |
| 85 | Spiritwalker's Grace (79206, 14.1 percent, 15 s, 2 min cooldown) |
| 87 | Ascendance (114049; Elemental form 114050: Lava Burst has no cooldown, Chain Lightning becomes Lava Beam 114074) |
| 90 | tier 6 talent (Elemental Blast 117014: 2.0 s cast, 12 s cooldown) |

An Elemental shaman does **not** know Fire Nova (Enhancement, 44), Stormstrike, Lava Lash, Primal Strike's replacement, Feral
Spirit, Spirit Walk, Windfury Weapon, Maelstrom Weapon, Healing Wave, Riptide, Earth Shield, Earthliving Weapon, Purify Spirit
or Greater Healing Wave (Enhancement and Restoration rows in SpecializationSpells).

### Casting while moving (data)

`SpellInterrupts.dbc` InterruptFlags: Lightning Bolt 14 (no movement bit), Chain Lightning, Lava Burst, Healing Surge and others
15 (bit 1 is "interrupted by movement"), Elemental Blast 47. So **Lightning Bolt can be cast while moving** in this client data
(its spell text says so), the only damage spell with a cast time that can; the other casts need Spiritwalker's Grace (aura 330,
level 85). The bots refuse any spell with a cast time while moving (`AI/PlayerbotAI.cpp:3850` and `:3922`: `CastingTime >
0 && bot->isMoving()`), so a moving Elemental casts nothing; the line `earth shock` "cast during movement" in the default list is
commented out (`CasterShamanStrategy.cpp:44`). Gap 9.

## How the bots run this specialisation (read)

- An Elemental shaman gets the `ele` combat strategy (`CasterShamanStrategy`, `AiFactory.cpp:391`), plus `totems`, `dps
  assist`, `cure`, `aoe` (`:397`) and the shared `racials`, `chat`, `default`, `cast time`, `potions`, `duel`, `boost` (`:316`).
  Out of combat: `nc` (`ShamanNonCombatStrategy`), `dps assist`, `cure` and the shared food, follow and loot strategies. A solo
  Restoration shaman runs `caster` as well (`:621`); that is the same class.
- Rank of an action (read, `strategy/Strategy.h`): ACTION_DEFAULT 5, NORMAL 10, HIGH 20, INTERRUPT 40, EMERGENCY 90; the engine
  takes the highest action that is useful and possible; an action whose spell is unknown is skipped. Health thresholds
  (`conf.dist:670-676`): critical 25, low 45, medium 65; mana: low 15, medium 40, high 65. **The trigger named "high mana" means
  mana below 65 percent** (`GenericTriggers.cpp:121-124`).
- The Elemental's actions in rank order: Purge 50 (any purgeable buff on the target, cost 16.4 percent); Stone Bulwark Totem 40,
  Healing Tide Totem 39, Ancestral Guidance 38 and Healing Surge 37 (all four at low health, 45 percent); Heroism and
  Bloodlust 30 and Elemental Mastery 29; Wind Shear 23 (target and enemy healer) and Fire Elemental Totem 23, Ascendance 22.5 and
  Ancestral Swiftness 22 (the `boost` strategy); Water Shield 19.5 and Thunderstorm 19 (mana below 65 percent, or the target
  within 5 yd); totems 7 (Searing, Healing Stream); then Earthquake 5.8 (4 attackers within 8 yd of the target), Chain Lightning
  5.6 (2 attackers within 8 yd), Earth Shock execute 5.5 (true only for a target under 25 percent health and under 1,500
  health, that is a low-level mob at the end of its life; BT-025), Flame Shock 5.3, and the default list: Unleash Elements 5.25,
  Lava Burst 5.2, Elemental Blast 5.15, Lightning Bolt 5.1.
- **`boost` fires when the fight is hard or the bot is alone** (`BoostTrigger`, `GenericTriggers.cpp:416-424`): "balance" below
  50 percent, and the balance value is the group's summed levels over the attackers' level-weighted sum (elite x3, boss x20)
  (`value/AttackerCountValues.cpp:55-110`); with no group the player side is 0, so **a solo shaman uses Fire Elemental,
  Ascendance, Bloodlust and Heroism on every pull**, a group only on elites and bosses.
- The cast check refuses everything with a cast time while moving (see above). A Chain Lightning, Earthquake or other
  area action is braked in a group while any attacker's threat for the bot is 50 percent of the tank's or more, and every
  damage spell at 80 percent (`strategy/generic/ThreatStrategy.cpp:20-30`).
- Totem upkeep (`TotemsShamanStrategy.cpp:21-28`, triggers `ShamanTriggers.cpp:219-282`): a totem is dropped into an empty
  element slot (or one more than 30 yd from the bot) at relevance 7: Healing Stream (water slot, from 30) and Searing Totem
  (fire slot, from 16; Magma Totem only if Searing is unknown, which never happens after 16). Earth and air slots stay empty
  (the per-totem strategies `tremor`, `earthbind`, `grounding` exist but the factory adds none of them). Totemic Recall is cast
  out of combat when any totem stands and nobody in the group fights (`ShamanTriggers.cpp:406-463`, `NC:17-18`).

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived"
comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/strategy/
Classes/shaman/` at commit `4853da9`. `CS` is `CasterShamanStrategy.cpp`, `GS` is `GenericShamanStrategy.cpp`, `NC` is
`ShamanNonCombatStrategy.cpp`, `TS` is `TotemsShamanStrategy.cpp`, `SA` is `ShamanActions.h`.

### Levels 15 to 29

Known: Lightning Bolt, Earth Shock, Lightning Shield, Flametongue Weapon, Thunderstorm, Flame Shock, Purge, Searing Totem and Wind
Shear (16), Cleanse Spirit (18), Water Shield, Rolling Thunder and Fulmination (20), Frost Shock (22), Earthbind (26), Chain
Lightning (28). Lava Burst arrives at 34, so the whole kit is Lightning Bolt, Flame Shock and (with the shield) Earth Shock. A
pull fight is short; a Lightning Bolt costs 7.1 percent of base mana, so about 14 casts empty the bar (derived).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, imbue missing | Flametongue Weapon on the main hand (8024, 10) | yes: `NC:74-75` (22) |
| 2 | core | out of combat and in combat, no shield | **Lightning Shield (324, 8)** (both guides; Water Shield is the mana alternative from 20, question 3) | **missing:** an Elemental never casts Lightning Shield (grep of the edges); it casts Water Shield from 20 (`CS:98-99`, 19.5, and `NC:76-77`, 21), so from 8 to 19 it has no shield at all |
| 3 | core | the leader pulls | Lightning Bolt (403, 1) as the opening cast and the filler | yes: default 5.1 (`CS:43`) |
| 4 | core | none of my Flame Shock on the target, the fight is not about to end | Flame Shock (8050, 12) | yes: 5.3 (`CS:92-93`), `FlameShockTrigger` fires when the aura is gone (`beforeDuration` 0, `ShamanTriggers.cpp:340`), the action needs the target to live 6 s (`SA:369-373`) |
| 5 | good | a fight of more than about 10 s | Searing Totem (3599, 16) (derived: 5.9 percent is one Lightning Bolt, so a 3 s pull should not buy one) | partly: `TS:26-28` drops it on every pull; `CastSearingTotemAction` passes `needLifeTime` 0 (`SA:261-266`) |
| 6 | good | Lightning Shield at 3 or more charges (derived; the core allows 2, the guides wait for 6 to 7 for the most damage per shock) | Earth Shock (8042, 6), Fulmination (88766, 20) | **missing as a charge spender:** the default Earth Shock is commented out (`CS:44`); the only Earth Shock is the execute edge, which fires for a target under 25 percent and under 1,500 health (`ShamanTriggers.cpp:346-361`; BT-025), so a low-level mob eats a 14.4 percent shock in its last seconds, with or without charges |
| 7 | good | an enemy casts | Wind Shear (57994, 16) | yes: 23 on the target and on an enemy healer (`GS:150-154`), interrupt trigger |
| 8 | good | mana below 65 percent, off cooldown | Thunderstorm (51490, 10): +about 15 percent mana, 45 s | yes: 19 (`CS:100-101`), and 19 again with the target within 5 yd (`:104-105`). Before the minor glyph (25) it knocks the pack off the tank (derived hazard, to check live) |
| 9 | good | health under 45 percent | Healing Surge on self (8004, 7), 34.3 percent mana | yes: 37 (`CS:68-69`) |
| 10 | good | the target carries a purgeable buff | Purge (370, 12), 16.4 percent mana | yes: `GS:160`, ACTION_DISPEL 50, no mana test |
| 11 | good | 3 or more attackers within 8 yd of the target, mana above 40 percent | Chain Lightning (421, 28), 30.5 percent mana (derived: one Chain Lightning costs 4.3 Lightning Bolts and hits at most 3 targets, so it is a burst button, not an efficiency one) | **wrong threshold and order from 28:** "light aoe" is 2 attackers and the edge is 5.6 (`GS:278-279`), above Flame Shock 5.3 and Lava Burst; no mana test |
| 12 | good | a curse on me or a member | Cleanse Spirit (51886, 18) | yes: `GS:191-195` (24, 23) |
| 13 | expert | a Flame Shock is running out (3 s left) | refresh it (wowtbc: early refresh allowed) | missing: recast only when the aura is gone |

Before the spells are learned (derived): at 15 to 19 the order is Flametongue, Lightning Shield, Lightning Bolt, Flame Shock; at
20 the shield's charges and Earth Shock become a button; at 22 Frost Shock is known but no line casts it (fine in a group;
a solo Elemental could root with Frozen Power, owner question 1). Names the strategy asks for that do not resolve are in "Spell
names" below.

### Levels 30 to 44

As above, plus the tier 2 talent (30), Healing Stream Totem and Totemic Recall (30), Lava Burst (34), Magma Totem (36), Grounding
Totem (38), Elemental Focus and Burning Wrath (40).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 14 | core | off cooldown, my Flame Shock on the target | Lava Burst (51505, 34) (both guides: with a Lava Surge proc at once; it always crits) | yes: default 5.2 (`CS:41`); below 34 the default asks for the unknown name every tick (spell names below) |
| 15 | core | Lava Burst before Lightning Bolt, Flame Shock before Lava Burst | the order Flame Shock, Lava Burst, Lightning Bolt (Method and wowtbc) | yes for single targets; with 2 attackers Chain Lightning 5.6 outranks both (line 11) |
| 16 | good | out of combat, a totem stands | Totemic Recall (36936, 30): 25 percent of each totem's cost back | yes: `NC:17-18` (60) |
| 17 | good | never for a damage dealer (derived) | **do not drop Healing Stream Totem** (5394, 30): 23.5 percent of base mana per 30 s of fighting for a small heal; neither guide puts it in a damage rotation (Method's glyph note is a raid cooldown) | **wrong:** `TS:21-22` drops it at relevance 7 whenever the water slot is empty. One drop costs as much as 3.3 Lightning Bolts, and the Elemental has no cost cut (derived) |
| 18 | good | health under 45 percent | Healing Surge on self (as above) | yes |

Magma Totem (36) is an Enhancement tool; for a caster that stands at range it would sit away from the pack: not for this
specialisation (derived).

### Levels 45 to 59

As above, plus the tier 3 talent (45), Lava Surge and Mail Specialization (50), Tremor Totem (54), Elemental Oath (55), Earth Elemental
Totem (58).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 19 | core | Lava Surge procs (the cooldown resets, the next Lava Burst is instant) | Lava Burst at once (both guides) | works by itself: the default asks for Lava Burst every tick and the cooldown reset makes it castable (derived from the spell text; to check live) |
| 20 | good | I need to survive a hard hit | Astral Shift (108271, tier 1 talent): -40 percent damage for 6 s | **missing** (no line; gap 7) |
| 21 | expert | a fear caster in the pack | Tremor Totem (8143, 54) | missing (the `tremor` strategy exists, unused) |
| 22 | expert | a boss or elite fight and Fire Elemental is on cooldown | Earth Elemental Totem (2062, 58) | missing; wowtbc lists it as a filler, not picked (derived) |

### Levels 60 to 74

As above, plus Earthquake and Healing Rain (60), the tier 4 talent, Capacitor Totem (63), Shamanistic Rage and Healing Tide Totem (65),
Fire Elemental Totem (66), Bloodlust and Heroism (70), Bind Elemental (72).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 23 | good | 6 or more attackers inside the circle (Method and wowtbc), mana above 65 percent | Earthquake (61882, 60): 70.3 percent of base mana, 2.5 s | **wrong:** 4 attackers within 8 yd of the target, rank 5.8, no mana test (`CS:72-73`, `SA:599-604`); one cast is 70 percent of the bar, and the cooldown is 10 s |
| 24 | core | health under 45 percent | Healing Surge; Healing Tide Totem (65) in the water slot | partly: both at low health (`CS:64-69`); Astral Shift missing |
| 25 | good | mana below 15 percent | Shamanistic Rage (30823, 65): 15 s of free spells and -30 percent damage | **missing for the Elemental** (the Enhancement strategy has it, `MeleeShamanStrategy.cpp:113-117`) |
| 26 | good | a hard fight (boss or elite) | Fire Elemental Totem (2894, 66) with Elemental Mastery and Bloodlust (70) | yes: `boost` (`GS:245-247`, 23), solo on every pull; gate `needLifeTime` 0 |
| 27 | expert | pre-pull | Searing Totem only without Fire Elemental | partly: the totem trigger treats a standing Fire Elemental as holding the slot (`ShamanTriggers.cpp:219-235`) |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | tier 5 Ancestral Guidance; Hex (75) as crowd control | talent edge at low health; no `cc` strategy for the shaman, no Hex line |
| 78 | Stormlash Totem on cooldown, with Bloodlust (Method opener) | missing (no action, no trigger) |
| 80 | Mastery: Elemental Overload; Method: Mastery above Haste | passive; stat weights (see Stat order) |
| 81 | Unleash Elements on cooldown before Lava Burst (Method order; wowtbc: before Lava Burst) | yes: default 5.25 (`CS:40`, action targets the current target, `ShamanActions.cpp:14-17`) |
| 85 | Spiritwalker's Grace when a cast must be made on the move | missing; the cast check would still refuse (it does not read aura 330) |
| 87 | Ascendance on cooldown (wowtbc: Flame Shock 15 s or more left, Lava Burst on cooldown, Unleash Elements active) | `boost` edge 22.5 without the condition (`GS:249-250`) |
| 90 | Method opener and single target: Lightning Shield, Flametongue, Flame Shock, Fire Elemental, Stormlash, Ascendance, Elemental Mastery, Lava Burst on proc, Elemental Blast, Earth Shock above 6 charges, Searing Totem without Fire Elemental, Lightning Bolt | missing Lightning Shield, Earth Shock, Stormlash; Elemental Blast needs the talent (config) |

## Cooldowns and defensives

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Elemental Mastery | talent 60 | on cooldown | on cooldown (a 20 s haste window every 90 s) | yes, always when ready (`CS:94-95`) |
| Fire Elemental Totem | 66 | on cooldown | boss, elite or hard pack | yes, `boost` |
| Ascendance | 87 | on cooldown with conditions | boss or elite | yes, `boost` without the conditions |
| Stormlash Totem | 78 | on cooldown | with Bloodlust | missing |
| Bloodlust / Heroism | 70 | raid cooldown | boss or elite | yes, `boost` (30) |
| Astral Shift | talent 15 | Method defensive | health under 45 percent in a group fight | missing |
| Healing Tide Totem | 65 | wowtbc defensive | the group is hurt | self health only (`CS:64-65`) |
| Ancestral Guidance | talent 75 | Method recommended | members hurt (heals 3 nearby) | self health only (`CS:66-67`) |
| Shamanistic Rage | 65 | wowtbc defensive | mana under 15 percent, or health under 25 | missing for the Elemental |
| Thunderstorm | 10 | not named (minor glyph) | mana tool every 45 s | yes |
| Wind Shear | 16 | not named | every interruptible cast of the pull target | yes |
| Purge | 12 | not named | a buff worth removing, mana permitting | yes, no mana test |
| Earthbind, Frost Shock | 26, 22 | not PvE | PvP and kiting only | no lines (PvP is future work) |

## Healer triage and mana (not a healer)

Healing that exists: Healing Surge on self at low health, and the out-of-combat "party member" heals (`NC:34-55`), which ask for
Riptide and Healing Wave first (Restoration only: unknown names for an Elemental) and then Healing Surge and Chain Heal.

Mana: no regeneration talent. Costs (percent of base mana): Lightning Bolt 7.1, Lava Burst 7.7, Flame Shock 11.9, Earth Shock 14.4,
Purge 16.4, Frost Shock 21.1, Chain Lightning 30.5, Healing Stream Totem 23.5, Healing Surge 34.3, Earthquake 70.3, Fire Elemental
26.9; Thunderstorm is free and gives about 15 percent back; Totemic Recall gives 25 percent of each recalled totem back; Rolling
Thunder gives 2 percent per proc; Shamanistic Rage (65) makes 15 s free; the generic drink rule is `AiPlayerbot.DungeonDrinkMana =
50` (`conf.dist:2272`). Derived rule: Lightning Bolt and Lava Burst are the sustainable cast; Chain Lightning, Earthquake, Healing
Stream and Purge are paid for with the mana the next pull needs.

## Spell names the Elemental strategies ask for that an Elemental does not know

`SpellIdValue` logs `[SID] ... qualifier='<name>' resolved=0` whenever a name matches no known, non-passive spell
(`strategy/value/SpellIdValue.cpp`) and caches the answer 20 s per name and bot. From the code (inference, not checked against the
log), the names an Elemental of level 15 to 59 asks for and does not know. Trigger edges that test an aura but no spell (the
`BuffTrigger` family, `GenericTriggers.cpp:212-225`) ask every tick.

| Name | Known from (Elemental) | Asked from |
|------|------------------------|------------|
| `fire nova` | never (Enhancement, 44) | `GS:272` ("medium aoe", 3 attackers), the magma node `CS:24` |
| `lava burst` | 34 | default, every tick of every fight 15 to 33 (`CS:41`) |
| `unleash elements` | 81 | default, every tick 15 to 80 (`CS:40`) |
| `elemental blast` | talent 90, only the Elemental Blast pick | default, every tick 15 to 89, and at 90 with Unleashed Fury (`CS:42`) |
| `chain lightning` | 28 | `GS:279` (2 attackers) at 15 to 27 |
| `earthquake` | 60 | `CS:73` (4 attackers) at 15 to 59 |
| `water shield` | 20 | `CS:99` every tick of every fight at 15 to 19 (the trigger has no known-spell test); `NC:77` out of combat |
| `stone bulwark totem` | talent 15, only that pick | `CS:60` at low health (never known with the premade) |
| `healing tide totem` | 65 | `CS:65` at low health, 15 to 64 |
| `ancestral guidance` | talent 75 | `CS:67` at low health, 15 to 74 |
| `fire elemental totem` | 66 | `GS:247` (`boost`, every pull when alone) at 15 to 65 |
| `ascendance` | 87 | `GS:250` (`boost`) at 15 to 86 |
| `heroism`, `bloodlust` | 70 | `GS:233-234` (`boost`) at 15 to 69 |
| `ancestral swiftness` | talent 60, only that pick | `GS:240-241` (`boost`, every pull when alone) |
| `riptide`, `riptide on party`, `healing wave on party` | never (Restoration) | `NC:34-48` when a party member is hurt out of combat |
| `healing stream totem`, `searing totem` | 30, 16 | gated by a known-spell test in the totem triggers (`ShamanTriggers.cpp:51-55`, `:198`, `:221`): no spam |
| `elemental mastery`, `totemic recall` | talent 60, 30 | gated by `HasSpell`: no spam |
| `water walking` | 24 | `NC:31` when swimming, at 15 to 23 |

Fix direction: give the default list and the `boost`, `aoe` and shield edges a known-spell test (the pattern of the totem
triggers), delete the Enhancement and Restoration names from the Elemental paths, and the `resolved=0` count per bot should
fall to near zero.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Lava Burst arrives at 34 (not 15), Chain Lightning at 28, Earthquake at 60, Fire Elemental at 66,
  Stormlash at 78, Unleash Elements at 81, Ascendance at 87, Elemental Blast at 90; below those levels the lists above start
  without them.
- Method's glyph of Spirit Walk is an Enhancement glyph; an Elemental shaman has no Spirit Walk.
- Fulmination here consumes every charge above one (core script); the guides' 6 to 7 is a damage-per-cast optimum.
- Lightning Bolt can be cast while moving in the 5.4.8 data (wowtbc names this only for Enhancement).
- Chain Lightning costs 30.5 percent of base mana, Earthquake 70.3 percent: the guides spend them freely because they play a
  level 90 mana pool with Rolling Thunder, Thunderstorm and Spirit; the leveling lists above gate them.
- Method lists "Elemental Blast" and Primal Elementalist as the tier 90 picks, both level 90 only; the bots' default list
  assumes Elemental Blast.
- Overgear, Icy Veins and Wowhead were not readable, so two guides were compared.

## Gap list (ordered)

Config (can go at once):

1. Talent link `AiPlayerbot.PremadeSpecLink.7.0` (`conf/playerbots.conf.dist:2819`) from `200010` to **`220012`** (Windwalk Totem at
   30, Elemental Blast at 90). The comment at `:2817-2818` needs the same change. Reason: Frozen Power is inert without a Frost
   Shock line, and the default list already asks for Elemental Blast. Tier 1 stays Astral Shift only if gap 7 is built.
2. Glyph order `PremadeSpecGlyph.7.0` (`:2820`) to `41531,44923,71155,43381,43385,41518` (Chain Lightning instead of
   Spiritwalker's Grace in the 75 slot, a level 85 spell the bots never cast). The Lightning Shield glyph needs gap 8.
3. Stat weights for the Elemental (`Mgr/Item/StatsWeightCalculator.cpp:560-569`): Intellect 1.5, Spirit 1.1, Mastery 0 below level
   80, keep Hit 1.1, Haste 1.0, Critical 0.8; add Spirit to the hit cap check (`:920-940`).

Strategy (class fixes, wait for the owner's in-game test), ranked by value for levels 15 to 60:

4. **Healing Stream Totem tax** (`TS:21-22`): remove the `no water totem` edge from the `totems` strategy for Elemental and
   Enhancement (keep it for Restoration in the heal strategy). Saves 23.5 percent of base mana per 30 s of fighting.
5. **Unknown-name spam** (the table above): a known-spell test on `FlameShockTrigger`/water shield/boost edges, level gates on
   the default list (`CS:40-44`: `lava burst` from 34, `unleash elements` from 81, `elemental blast` with the talent), delete
   `fire nova` from the Elemental `aoe` path (`GS:271-272`) and the magma node (`CS:24`), delete the Restoration names from
   the non-combat heal list for non-healers (`NC:34-48`). Verify with the debug log: `resolved=0` lines per bot fall to near zero.
6. **Chain Lightning and Earthquake thresholds** (`GS:278-279`, `CS:72-73`): Chain Lightning at 3 or more attackers within 8 yd, rank
   below Flame Shock and Lava Burst (5.25 or lower), mana above 40 percent; Earthquake at 6 or more attackers, mana above 65 percent.
7. **Astral Shift** (`talent 15`): a new action and a `low health` edge in both damage strategies (the pattern of
   `CastAncestralGuidanceAction`, `SA:621-626`); or change the tier 1 pick to Nature's Guardian (question 1).
8. **Shield**: Lightning Shield for the Elemental from level 8 (both guides), drop the `water shield` edges (`CS:98-99`,
   `NC:76-77`; the two shields replace each other, so both edges together would flip-flop), and add an Earth Shock edge at 6 or
   more charges (`Lightning Shield` aura stack, the `HasAuraStackTrigger` pattern used for Maelstrom Weapon). Owner decision first
   (question 3).
9. **Casting while moving** (shared code, `AI/PlayerbotAI.cpp:3850`, `:3922`): allow a spell whose `InterruptFlags` lacks the
   movement bit (Lightning Bolt) or that the bot has an aura 330 for (Spiritwalker's Grace, 85, then a cast edge for it).
10. **Searing Totem lifetime**: give `CastSearingTotemAction` a `needLifeTime` of 10 s (`SA:261-266`).
11. **Shamanistic Rage for the Elemental** at mana under 15 percent (65 and up), like the Enhancement edge (`MeleeShamanStrategy.cpp:113-114`).
12. **Thunderstorm before the glyph** (levels 10 to 24): cast it only when the bot is alone or the mana is below 40 percent (derived:
    it knocks the pack off the tank); check live what it does to a pack.
13. **Flame Shock refresh** (expert): `beforeDuration` of about 3 s on `FlameShockTrigger` (`ShamanTriggers.h`), and a Flame Shock on a
    second target at two attackers (Method and wowtbc).
14. **Ghost Wolf** (15) as a travel form when the group runs and the bot is not mounted: no code exists for it (only the removal on
    mounting, `AI/PlayerbotAI.cpp:3550`); low value.
15. Stormlash Totem (78), Earth Elemental (58), Tremor Totem (54), Capacitor Totem (63), Hex (75): later, with the 61 to 90 pass.
16. Skill tiers: the Elemental strategies have no tier gating (`BotSkill` is used in one shaman trigger only, the Restoration filler,
    `ShamanTriggers.cpp:503`). Lines marked good and expert above (Searing Totem lifetime, Earth Shock at charges, Thunderstorm
    timing, Purge, Cleanse Spirit, the Flame Shock refresh, Astral Shift) are the ones a casual bot should drop.
17. To check live: that Lava Surge resets Lava Burst for the default list; Thunderstorm at 10 to 24 against a pack; Chain Heal and
    Healing Rain for a non-Restoration shaman; Fulmination from 2 charges; the Searing Totem on a 3 s pull; whether `Water Shield`
    (19.5) and a future `Lightning Shield` edge fight; Bloodlust and Heroism both castable; `[SID]` counts per bot.

Owner questions:

1. Tier 1: Astral Shift (guide; needs gap 7, two lines of code) or Nature's Guardian (a passive that works with no code, but no
   control over when it triggers). Tier 2: Frozen Power for solo leveling (needs a Frost Shock line) or Windwalk Totem (guide).
2. Tier 90: Elemental Blast (Method single target and the bots' default list) or Primal Elementalist (Method, dungeons and AoE;
   the elementals need their own totem lines).
3. Shield: Lightning Shield (both guides, Fulmination, Rolling Thunder, glyph) or Water Shield (the mana shield the bots use
   today, from 20; nothing before 20). The Elemental's mana is its limit at levels 15 to 60 (Chain Lightning 30.5 percent), so
   Water Shield is the bot-friendly choice and Lightning Shield the guide choice.
4. Healing Stream Totem for damage specialisations: drop it (saves mana) or keep it for a healer-less group (it heals the most
   hurt member for 15 s).
5. Chain Lightning from 3 attackers (burst, derived) or from 2 (the guides, level 90 mana).

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 7 players of this specialisation (rating 2511 to 2805). 2v2: no player of this specialisation. Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** 3v3 (7 players)
- **BT-324 result:** updated (BT-450)
- **Set now:** `PremadeSpecLinkPvp.7.0 = 020110` (was `010110`), `PremadeSpecGlyphPvp.7.0 = 71155,43388,45778,43381,43385,41531`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Nature's Guardian 100% / - / 7 of 7 | Nature's Guardian | Nature's Guardian | matches |
| 2 (30) | Windwalk Totem 100% / - / 7 of 7 | Earthgrab Totem | Windwalk Totem | changed (7 of 7) |
| 3 (45) | Call of the Elements 100% / - / 7 of 7 | Call of the Elements | Call of the Elements | matches |
| 4 (60) | Ancestral Swiftness 100% / - / 7 of 7 | Ancestral Swiftness | Ancestral Swiftness | matches |
| 5 (75) | Ancestral Guidance 100% / - / 7 of 7 | Ancestral Guidance | Ancestral Guidance | matches |
| 6 (90) | Unleashed Fury 100% / - / 7 of 7 | Unleashed Fury | Unleashed Fury | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Lightning Shield (71155) | 100% | - | 7 of 7 | yes | yes |
| major | Healing Stream Totem (41533) | 57% | - | 4 of 7 | no | no |
| major | Thunder (45770) | 43% | - | 3 of 7 | no | no |
| major | Flame Shock (41531) | 29% | - | 2 of 7 | yes | yes |
| major | Shamanistic Rage (45776) | 29% | - | 2 of 7 | no | no |
| major | Hex (45777) | 29% | - | 2 of 7 | no | no |
| major | Totemic Vigor (45778) | 14% | - | 1 of 7 | yes | yes |
| minor | Totemic Encirclement (43388) | 71% | - | 5 of 7 | yes | yes |
| minor | the Spectral Wolf (43386) | 29% | - | 2 of 7 | no | no |
| minor | Astral Recall (43381) | 14% | - | 1 of 7 | yes | yes |
| minor | Far Sight (43385) | 0% | - | 0 of 7 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **What changed (3v3, 7 players, no 2v2 player):** tier 30 is Windwalk Totem (100 percent) instead of Earthgrab Totem. The other five tiers match. The talent spells strategy already casts Windwalk Totem when rooted or slowed (BT-322).
- **Left open:** the major slots (the set holds Totemic Vigor 14 and Flame Shock 29; the ladder has Healing Stream Totem 57, Thunder 43, no majority). Lightning Shield 100 and Totemic Encirclement 71 are held.

## PvP class package: bots today (BT-677 to BT-681, 2026-11-19)

Status of the digest's section 12 rules after the Elemental package (see `docs/pvp/class-packages.md`; all wait for the owner's in-game test):

| Digest rule | Bots today |
|---|---|
| 1 Flame Shock on every enemy | partly, as before (the main target) |
| 2 Ancestral Swiftness then Hex the healer | done (BT-677); the Hex itself is the control chain (BT-629); no check for a curse remover |
| 3 burst when the healer is held | done as a hold for the burst window (BT-680); Unleash Elements and Lava Burst are the rotation |
| 4 Grounding on an incoming control | done (BT-678) |
| 5 Shamanistic Rage when stunned | not built |
| 6 school lock fallbacks | not built (the nature lock falls through to Lava Burst) |
| 7 Wind Shear on a heal or control cast | the shared interrupt gate (BT-521) |
| 8 Thunderstorm on a melee on top | as before (`safe thunderstorm`) |
| 9 Spiritwalker's Grace while moving | done (BT-681) |
| 10 off-heals | the off-heal policy of the module |
| 11 Windwalk Totem when snared | the talent policy edge (freedom) |
| 12 Purge by priority | not built |

Set: unchanged. Ancestral Swiftness and Call of the Elements (100 percent each) now have rules.
