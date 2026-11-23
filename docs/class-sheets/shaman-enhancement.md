# Shaman: Enhancement (damage, melee)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). This pass added no
  rotation change. The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Enhancement Shaman Class Guide, MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 23 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image; the text lists the tiers in order without levels).
  - wowtbc.gg, "Enhancement Shaman" MoP Classic PvE guide (no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellInterrupts, SpellCategories, ChrClasses; `data/db2/Item.db2` and
    `Item-sparse.db2` for glyph item ids) through `spec_data.py` and short read-only scripts; the core source (the SkyFire core: `scripts/Spells/spell_shaman.cpp`, `Entities/Unit/StatSystem.cpp`) for the spell scripts and attack power.
  - Not readable: Overgear and Icy Veins (HTTP 403, as noted in the skill), Wowhead (needs a browser).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's
  marks), 61 to 90 briefly. The Elemental sheet (`shaman-elemental.md`) holds the facts the two share (talent notes, totem costs).

Which source was richer: **the stat order with the caps, the dungeon talent note, the glyph rationale (Fire Nova, Spirit Walk,
Lightning Shield), the cooldown list** Method; **the option list of all 18 talents, the Maelstrom Weapon rules, the opener with a
pre-pot, the 2 to 5 and 6+ area split, the defensives, the moving rule** wowtbc. Neither guide has levelling advice, a mana
plan, a threat rule or a word on what an Enhancement shaman does before Stormstrike (26), Windfury Weapon (30), Fire Nova (44) or
Maelstrom Weapon (50), so those lines come from the 5.4.8 data and are marked "derived". The guides disagree on five points:

- **When to spend Maelstrom Weapon.** Method: Lightning Bolt at 5 stacks, lower stacks only as the last filler; wowtbc: Elemental
  Blast at 1 or more stacks and Lightning Bolt at 2 or more, when no auto attack is delayed. Pick Method's 5 for the core line
  (derived: each stack takes 20 percent off cast time and mana cost, effect data of 53817, so 5 stacks is instant and free and 4
  is 0.5 s; spending earlier delays the white swing and spends the stacks cheaply). The bots spend at 4 (19.5) and 5 (20), which
  is within the same class of play.
- **Tier 90.** Method: Unleashed Fury or Primal Elementalist (Elemental Blast is not mentioned for Enhancement); wowtbc: its
  rotation and opener use Elemental Blast (tagged T15+). Pick Method's Unleashed Fury (guide), question 2.
- **Order of Earth Shock.** Method: after Unleash Elements; wowtbc: after Unleash Elements as the last button. Same.
- **Opener.** Method: Spirit Walk to the target, Feral Spirit, Unleash Elements, Flame Shock; wowtbc: Feral Spirit first, then
  Fire Elemental, Elemental Blast pre-cast. All level 60 or higher; no pick needed below 60.
- **Tier 60.** Method: Elemental Mastery for single target, Echo of the Elements for sustained cleave, and its **dungeon build
  names Echo of the Elements**; wowtbc lists both. Question 1.

## Talents

Tiers and columns are from Talent.dbc (shared by the three shaman specialisations). "Bots today" is
`AiPlayerbot.PremadeSpecLink.7.1 = 200010` (`conf/playerbots.conf.dist:2824`; one digit per tier, column counted from 0): Astral
Shift, Frozen Power, Call of the Elements, Elemental Mastery, Ancestral Guidance, Unleashed Fury.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (200010) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Astral Shift (Method: locked) | Nature's Guardian (passive); Stone Bulwark Totem | 108271 / 30884 / 108270 | Astral Shift, same. **Inert: no line casts it** |
| 2 (30) | Windwalk Totem (Method default) | Earthgrab Totem for an AoE root | 108273 / 51485 / 63374 | **Frozen Power, differs and inert** (no Frost Shock line); Windwalk would be inert too, the change only aligns the premade with the guide |
| 3 (45) | Call of the Elements (Method default) | Totemic Projection when moving much | 108285 / 108287 / 108284 | Call of the Elements, same; inert (the edge was removed, `MeleeShamanStrategy.cpp:72-75`) |
| 4 (60) | Elemental Mastery for single target; **Echo of the Elements for the dungeon build** (Method) | Ancestral Swiftness viable in dungeons (Method) | 16166 / 108283 / 16188 | Elemental Mastery: a cast edge at 29, always when ready (`MeleeShamanStrategy.cpp:81-82`) |
| 5 (75) | Ancestral Guidance (Method: locked) | | 108281 | Ancestral Guidance, same: cast at low health (38, `:124-125`) |
| 6 (90) | Unleashed Fury (Method, single target) | Primal Elementalist for burst (Method); Elemental Blast (wowtbc rotation) | 117012 / 117013 / 117014 | Unleashed Fury, same. **But the default list asks for Elemental Blast first on every tick** (`:61`), a name this pick never knows |

Notes on the picks (data, not guide):

- Echo of the Elements (108283) is a passive (a chance to repeat a direct damage spell) and needs no line, so it is the
  bot-friendly tier 4 pick; Elemental Mastery (16166, +30 percent haste for 20 s, 90 s cooldown) has a line that casts it on
  every ready tick.
- Astral Shift (108271): damage taken -40 percent for 6 s, 90 s cooldown, no cost, usable while stunned. Nature's Guardian
  (30884): below 30 percent health, maximum health +25 percent for 6 s, 30 s cooldown, passive.
- Unleashed Fury (117012): with the Flametongue Weapon unleashed (Unleash Elements, level 81), the target takes more damage from
  Lightning Bolt and Lava Burst; with Windfury unleashed, auto attacks can proc Static Shock for a while (spell text).
  Unleash Elements is level 81, so the pick matters at 90 only.
- Ancestral Guidance (108281): 10 s, a share of the damage dealt is healed to up to 3 nearby hurt members, 2 min cooldown.

Proposed link (derived): **`220010`** = Astral Shift (needs gap 8), Windwalk Totem, Call of the Elements, Elemental Mastery,
Ancestral Guidance, Unleashed Fury. Only the tier 30 digit changes. Owner questions 1 and 2.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`, effect text from Spell.dbc. Slots unlock in
pairs at 25, 50 and 75 (config comment `conf/playerbots.conf.dist:2631-2636`). The config order is major, minor, major, minor,
minor, major. "Bots today" is `AiPlayerbot.PremadeSpecGlyph.7.1 = 41530,43381,71155,43385,43386,41524` (`conf.dist:2825`).

| Type | Glyph | Item id | Why (one clause) | Bots today |
|------|-------|--------:|------------------|------------|
| Major | Fire Nova | 41530 | both guides (Method: safe; wowtbc: standard); bigger radius (55450), useful from 44 | yes, slot 1 (level 25) |
| Major | Lightning Shield | 71155 | both guides (Method: mandatory); 10 percent less damage taken for 6 s after the shield triggers (142912) | yes, slot 3 (level 50); the bot casts Lightning Shield, so it works |
| Major | Spirit Walk | 41524 | both guides (movement); Spirit Walk cooldown shorter (55454) | yes, slot 6 (level 75); **dead until a line casts Spirit Walk** (gap 7), Spirit Walk is level 60 |
| Major, alternative | Chain Lightning | 41518 | both guides (4+ targets); two more jump targets, 10 percent less damage | no |
| Major, alternative | Feral Spirit | 45771 | wowtbc (self healing); the wolves' Spirit Hunt heals more | no |
| Major, alternative | Healing Storm | 43344 | both guides (sustain, raid healing); each Maelstrom Weapon stack adds direct healing (89646), which helps the Healing Surge the bots cast at low health | no |
| Major, alternative | Flame Shock | 41531 | Method (sustain); Flame Shock damage heals the shaman | no |
| Minor | Astral Recall, Far Sight, the Spectral Wolf | 43381, 43385, 43386 | cosmetic (shorter cooldown, usable indoors, wolf look) | yes, slots 2, 4, 5 |
| Minor, avoid | Lava Lash | 41540 | stops Lava Lash spreading Flame Shock (55444): worse | no |

Disagreement: Method calls Lightning Shield mandatory and lists Fire Nova, Spirit Walk, Feral Spirit, Flame Shock, Healing Storm
and Chain Lightning as optional; wowtbc has Fire Nova, Lightning Shield and Spirit Walk as standard. The premade equals wowtbc's
standard list. **No change proposed.** Only the Spirit Walk glyph waits for gap 7.

## Stat order

Method: Agility, then Mastery, then Haste, then Critical Strike; Hit and Expertise to about 7.5 percent each (the caps come
first); upgrades matter more than the type. wowtbc has no stat order. The caps are level 90 numbers and cannot be reached with
the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Agility first** (the data gives 2 attack power per Agility and 1 per Strength, `ChrClasses.dbc` for the shaman; Agility also
  raises critical strike), then Strength only as a distant second.
- **Hit and Expertise before the other secondary stats** until the caps (Method), then Haste, then Critical Strike.
- **Mastery last below level 80**: Mastery: Enhanced Elements (77223) is learned at 80 (SpecializationSpells), so before it a
  Mastery rating does nothing. From 80 Method ranks it above Haste.
- **Spell Power and Intellect are worth almost nothing**: Mental Quickness (30814, level 10) makes the shaman's spell power 65
  percent of attack power and "you no longer benefit from other sources of spell power"; Intellect only adds mana.
- Mail from level 40 (the Mail skill) and Mail Specialization at 50 for this role, so mail over leather from 40.
- Weapons: Dual Wield is learned at 10; two one-handed weapons, the slower the better for Windfury and Flametongue (the bots'
  calculator already prefers slow weapons, `StatsWeightCalculator.cpp:1139-1160`).
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:508-520`): Agility 1.4, **Strength 1.1** (about 0.8 of Agility, although a
  point of Strength gives half the attack power), Intellect 0.3, Attack Power 1.0, **Spell Power 0.5** (dead, see above), Hit 2.1,
  Critical 1.5, Haste 1.8, **Mastery 2.1 at every level** (dead below 80, and the highest of the secondary stats), Expertise 2.0.
  The hit overflow uses the spell hit type for Enhancement (`:131-132`); whether that cap equals Method's 7.5 percent is to
  check. Proposed (derived): Strength 0.7, Spell Power 0, Mastery 0 below 80 and 2.1 from 80; keep the rest.

## What the specialisation knows, by level (5.4.8 data)

An Enhancement shaman knows the Shaman class line (SkillLineAbility 924) plus the Enhancement rows of SpecializationSpells (spec
263). Ids are the spells the player casts. Costs are percent of base mana; **Mental Quickness (30814) cuts instant spells and
totems by 75 percent and shock spells by 90 percent** (effect data), so the figures below are the printed cost and the
discounted cost in brackets where it applies. The global cooldown is 1.5 s. Shocks share one 6 s cooldown. The specialisation
swap is **Stormstrike replaces Primal Strike** (level 26); `BotOverrideSpell` applies it (`AI/PlayerbotAI.cpp:3738-3753`), so the
name `primal strike` casts Stormstrike from 26.

| Level | Spell (id) |
|------:|------------|
| 1 | Lightning Bolt (403, 7.1 percent, 2.5 s; an Enhancement shaman has no Shamanism) |
| 3 | Primal Strike (73899, 9.4 [2.4] percent, 8 s cooldown) |
| 6 | Earth Shock (8042, 14.4 [1.4] percent, instant, 6 s shock cooldown) |
| 7 | Healing Surge (8004, 34.3 percent, 1.5 s, not instant, so no discount) |
| 8 | Lightning Shield (324, no cost, 60 min) |
| 10 | Flametongue Weapon (8024, 7.0 [1.8] percent); **Lava Lash** (60103, 4.0 [1.0] percent, 10 s cooldown, extra damage with a Flametongue off-hand, spreads Flame Shock to 4 enemies); Dual Wield (86629); Mental Quickness (30814); Primal Wisdom (51522: melee hits restore mana) |
| 12 | **Flame Shock** (8050, 11.9 [1.2] percent, 30 s DoT); Purge (370) |
| 14 | Ancestral Spirit (2008) |
| 15 | Ghost Wolf (2645, 7.2 percent, +30 percent speed); tier 1 talent |
| 16 | Searing Totem (3599, 5.9 [1.5] percent, 60 s); **Wind Shear** (57994, 9.4 [2.4] percent, 12 s cooldown) |
| 18 | Cleanse Spirit (51886, curses) |
| 20 | Flurry (16282); Water Shield (52127) |
| 22 | Frost Shock (8056) |
| 24 | Water Walking (546) |
| 26 | **Stormstrike** (17364, 9.4 [2.4] percent, 8 s cooldown, replaces Primal Strike); Earthbind Totem |
| 28 | Chain Lightning (421, **30.5 percent**, 2.0 s, 3 s cooldown) |
| 30 | **Windfury Weapon** (8232, 8.2 [2.1] percent); Healing Stream Totem (5394, 23.5 [5.9] percent); Totemic Recall (36936); tier 2 talent |
| 32 | Reincarnation |
| 34 | Searing Flames (77657: Searing Totem and Fire Elemental hits make Flametongue stronger; Lava Lash spends them) |
| 36 | Magma Totem (8190, 21.1 [5.3] percent, 60 s); Far Sight |
| 38 | Grounding Totem |
| 40 | Static Shock (51527: Stormstrike and Lava Lash can deal a Lightning Shield orb); Burning Wrath; Mail |
| 44 | **Fire Nova** (1535, 13.7 [3.4] percent, 4 s cooldown: every enemy carrying your Flame Shock emits a wave that hits the others around it, so it does nothing against one enemy); Chain Heal (class line) |
| 45 | tier 3 talent |
| 46 | Frostbrand Weapon (8033) |
| 50 | **Maelstrom Weapon** (51530: melee hits give stacks of 53817, up to 5; each stack -20 percent cast time and mana cost of a Nature spell with a cast time); Mail Specialization |
| 54 | Tremor Totem |
| 55 | Unleashed Rage (30809: +attack speed for the group) |
| 58 | Earth Elemental Totem (2062) |
| 60 | **Feral Spirit** (51533, 12.0 [3.0] percent, 2 min cooldown, 30 s, two Spirit Wolves); **Spirit Walk** (58875, 1.5 percent, 15 s, 60 s cooldown: breaks movement impairment, +speed); Healing Rain (class line); tier 4 talent |
| 63 | Capacitor Totem |
| 65 | Shamanistic Rage (30823, 15 s, 60 s cooldown: -30 percent damage taken, spells cost nothing); Healing Tide Totem (108280) |
| 66 | Fire Elemental Totem (2894, 26.9 percent, 5 min cooldown) |
| 70 | Bloodlust (2825), Heroism (32182) |
| 72 | Bind Elemental |
| 75 | Hex (51514); tier 5 talent |
| 78 | Stormlash Totem (120668, 5 min cooldown) |
| 80 | Mastery: Enhanced Elements (77223); Grace of Air |
| 81 | Unleash Elements (73680, 8.2 [2.1] percent, 15 s cooldown) |
| 85 | Spiritwalker's Grace (79206) |
| 87 | Ascendance (114049; Enhancement form 114051: Stormstrike becomes Stormblast 115356, melee reaches farther) |
| 90 | tier 6 talent |

An Enhancement shaman does **not** know Lava Burst, Earthquake, Thunderstorm, Shamanism, Healing Wave, Riptide, Earth Shield,
Earthliving Weapon, Purify Spirit or Greater Healing Wave (Elemental and Restoration rows).

## How the bots run this specialisation (read)

- An Enhancement shaman gets the `enh` combat strategy (`MeleeShamanStrategy`, `AiFactory.cpp:395`; the other branch of the
  `if`, so a shaman with no specialisation at levels 1 to 9 runs it as well), plus `totems`, `dps assist`, `cure`, `aoe` (`:397`)
  and the shared `racials`, `chat`, `default`, `cast time`, `potions`, `duel`, `boost` (`:316`). Out of combat: `nc`
  (`ShamanNonCombatStrategy`), `dps assist`, `cure`, food, follow, loot.
- Rank of an action (read, `strategy/Strategy.h`): ACTION_DEFAULT 5, NORMAL 10, HIGH 20, INTERRUPT 40, EMERGENCY 90; the engine
  takes the highest action that is useful and possible; an action whose spell is unknown is skipped. Health thresholds
  (`conf.dist:670-676`): critical 25, low 45, medium 65; mana: low 15, medium 40, high 65. The trigger named "high mana" means
  mana below 65 percent.
- The Enhancement's actions in rank order: Purge 50 (any purgeable buff on the target) and Spirit Walk 50 (a trigger that waits
  for wolves); Reach melee 40 (enemy out of melee); Healing Tide Totem 39, Ancestral Guidance 38 and Healing Surge 37 (low
  health, 45 percent); Heroism and Bloodlust 30; Elemental Mastery 29; Fire Elemental Totem (melee) 24 (`boost`); Wind Shear 23,
  Fire Nova 23 (3 attackers within 8 yd) and Shamanistic Rage 23 (mana below 40 percent or health below 45 percent); Ascendance
  22.5 (`boost`); Ancestral Swiftness 22; Chain Lightning 22 and 21 (5 and 4 Maelstrom stacks with 3 attackers); Lightning Bolt 20
  and 19.5 (5 and 4 stacks); Flame Shock 19; Lightning Shield 18.5; totems 7 (Searing, Healing Stream); then the default list:
  Elemental Blast 5.6, Unleash Elements 5.55, Stormstrike 5.5, Feral Spirit 5.4, **Earth Shock 5.3, Lava Lash 5.2**, Primal
  Strike 5.1 (and Fire Nova 5.1 from `aoe` when in melee), melee 5.
- **`boost` fires when the fight is hard or the bot is alone** (`BoostTrigger`, `GenericTriggers.cpp:416-424`): "balance" below
  50 percent of the group's summed levels against the level-weighted attackers (elite x3, boss x20,
  `value/AttackerCountValues.cpp:55-110`); with no group the player side is 0, so a solo shaman uses Fire Elemental, Ascendance,
  Bloodlust and Heroism on every pull, a group only on elites and bosses.
- The Maelstrom Weapon triggers (`MaelstromWeaponTrigger`, `ShamanTriggers.h:36-41`) read the aura named "Maelstrom Weapon" with
  at least N stacks on the shaman; the passive (51530) has no duration and is skipped by the duration test
  (`HasAuraStackTrigger`, `GenericTriggers.cpp:521-527`), so only the 53817 buff counts. Before level 50 neither trigger can fire.
- Imbues are out-of-combat edges only (`NC:81-89`): Windfury Weapon on the main hand (its node falls back to Flametongue before
  30, `GenericShamanStrategy.cpp:93-99`), Flametongue Weapon on the off-hand, each when the hand has no imbue
  (`ShamanTriggers.cpp:144-191`; the main hand trigger also replaces a wrong imbue after a respec, at most every 5 min).
- A Chain Lightning or Fire Nova action is braked in a group while any attacker's threat is 50 percent of the tank's or more,
  and every damage spell at 80 percent (`strategy/generic/ThreatStrategy.cpp:20-30`).
- Totem upkeep: the same as the Elemental (`TotemsShamanStrategy.cpp:21-28`): Healing Stream (water slot, from 30) and Searing
  Totem (fire slot, from 16) at relevance 7 into an empty slot; Magma Totem is the fire-slot fallback if Searing is unknown, so it
  is never dropped; the earth and air slots stay empty. Totemic Recall out of combat (`NC:17-18`).

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived"
comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/strategy/
Classes/shaman/` at commit `4853da9`. `MS` is `MeleeShamanStrategy.cpp`, `GS` is `GenericShamanStrategy.cpp`, `NC` is
`ShamanNonCombatStrategy.cpp`, `TS` is `TotemsShamanStrategy.cpp`, `SA` is `ShamanActions.h`.

### Levels 15 to 29

Known: Lightning Bolt, Earth Shock, Lightning Shield, Flametongue Weapon, Lava Lash, Primal Strike (until 26), Flame Shock, Purge,
Searing Totem and Wind Shear (16), Cleanse Spirit (18), Flurry and Water Shield (20), Frost Shock (22), Stormstrike (26), Chain
Lightning (28). Every attack button except Lightning Bolt and Chain Lightning is instant and discounted by Mental Quickness, so mana
is a minor limit for this specialisation; the melee swing is the damage. The bot stays in melee range (reach melee 40).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, imbue missing | Flametongue Weapon on both hands (8024, 10); Windfury Weapon replaces the main-hand one at 30 | yes: `NC:81-86` (22 main hand, 21 off hand), main-hand fallback to Flametongue (`GS:93-99`). Needs an off-hand weapon for the second imbue |
| 2 | core | no shield | Lightning Shield (324, 8) | yes: 18.5 in combat (`MS:92-93`), 20 out of combat (`NC:87-89`) |
| 3 | core | none of my Flame Shock on the target and the fight is not about to end | Flame Shock (8050, 12) | yes: 19 (`MS:89-90`), recast when the aura is gone |
| 4 | core | off cooldown | Stormstrike (17364, 26); before 26 Primal Strike (73899, 3) in its place | partly: Stormstrike 5.5 (`MS:63`) and Primal Strike 5.1 (`:65`); both are right, but Primal Strike sits **below** Earth Shock and Lava Lash (lines 5 and 6) |
| 5 | core | off cooldown | Lava Lash (60103, 10): above Earth Shock (Method and wowtbc order) | **wrong order:** 5.2 (`MS:64`), below Earth Shock 5.3 |
| 6 | good | the shock cooldown is free and Flame Shock holds | Earth Shock (8042, 6) as the cheap filler, 1.4 percent of base mana | **wrong order:** 5.3 (`MS:64`), above Lava Lash and Primal Strike; it also eats the shock cooldown Flame Shock needs (BT-024, left as is) |
| 7 | good | a fight of more than about 10 s | Searing Totem (3599, 16); Searing Flames (34) feeds Lava Lash | partly: `TS:26-28` drops it on every pull; `CastSearingTotemAction` passes `needLifeTime` 0 (`SA:261-266`) |
| 8 | good | an enemy casts | Wind Shear (57994, 16) | yes: 23 (`GS:150-154`) |
| 9 | good | health under 45 percent | Healing Surge on self (8004, 7), 34.3 percent | yes: 37 (`MS:126-127`) |
| 10 | good | the target carries a purgeable buff | Purge (370, 12) | yes: `GS:160`, rank 50, no mana test (16.4 percent printed, 4.1 after the discount) |
| 11 | good | a curse on me or a member | Cleanse Spirit (51886, 18) | yes: `GS:191-195` |
| 12 | expert | derived: the group runs and the bot is not mounted, or the bot is rooted | Ghost Wolf (2645, 15) as a travel form; Spirit Walk (58875, 60) to break a root | missing, see gaps 7 and 12 |

Before the spells are learned (derived): 15 to 25 the buttons are Flame Shock, Lava Lash, Primal Strike and Earth Shock; Stormstrike
takes over at 26 and Lava Lash stays behind it. Chain Lightning is not a button below 50 (2.0 s and 30.5 percent without Maelstrom
Weapon).

### Levels 30 to 44

As above, plus Windfury Weapon, Healing Stream Totem and Totemic Recall (30), the tier 2 talent, Searing Flames (34), Magma Totem (36),
Grounding Totem (38), Static Shock (40), Fire Nova (44).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 13 | core | out of combat, no Windfury | Windfury Weapon on the main hand (8232, 30), Flametongue stays on the off-hand | yes: `NC:81-83` (22), `windfury weapon main hand` node with the Flametongue fallback before 30 |
| 14 | good | never for a damage dealer (derived) | **do not drop Healing Stream Totem** (5394, 30): 5.9 percent after the discount, 23.5 printed, a small heal | **wrong:** `TS:21-22` drops it at relevance 7 whenever the water slot is empty; for Enhancement the cost is small, for the Elemental it is large |
| 15 | good | out of combat, a totem stands | Totemic Recall (36936, 30) | yes: `NC:17-18` |
| 16 | good | 3 or more attackers within 8 yd of the bot and a fight of more than about 15 s | Magma Totem (8190, 36), 5.3 percent after the discount, replaces Searing (Method: more than 5 enemies; derived: 3 for dungeon packs because the totem is cheap here) | **missing:** `TS:27-28` lists it as the second fire edge, never reached; the melee AoE strategy has the edge commented out (`MS:132-133`) and is not added anyway |
| 17 | good | 2 or more enemies carry my Flame Shock within 10 yd of the target | Fire Nova (1535, 44), 3.4 percent | **wrong:** "medium aoe" 3 attackers at 23 (`GS:284`) and "enemy within melee" at 5.1 (`GS:289-290`) with only the target's Flame Shock tested (`ShamanActions.cpp:48-60`), so it fires at a single enemy, where it does nothing (spell text of 1535) |
| 18 | good | Lava Lash on a Flame Shocked target with other enemies near | Lava Lash spreads Flame Shock to 4 enemies (60103) | works through the default list (5.2) |
| 19 | core | health under 45 percent | Healing Surge; Healing Tide (65) | yes |

### Levels 45 to 59

As above, plus the tier 3 talent (45), Frostbrand (46), **Maelstrom Weapon (50)**, Mail Specialization (50), Tremor Totem (54), Unleashed Rage
(55), Earth Elemental Totem (58).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | core | 5 Maelstrom Weapon stacks | Lightning Bolt (403): instant and free (Method: at 5; the bots use 4 and 5) | yes: 20 at 5 stacks, 19.5 at 4 (`MS:94-98`); both above Flame Shock 19 |
| 21 | good | 3 or more attackers within 8 yd, 4 or 5 stacks | Chain Lightning (421, 28) instead of Lightning Bolt (both guides) | yes: 22 and 21 (`GS:285-288`) |
| 22 | good | health under 70 percent, 3 or more stacks, alone (derived) | Healing Surge (8004): 3 stacks make it 0.6 s and 13.7 percent, 5 stacks instant and free; the glyph of Healing Storm adds healing per stack | **missing:** Healing Surge only at 45 percent and ignores the stacks |
| 23 | good | an enemy casts | Wind Shear; Purge gives a stack with the Glyph of Purging (104052, major, not in the premade) | yes (Wind Shear) |
| 24 | expert | a fear caster in the pack | Tremor Totem (8143, 54) | missing |

### Levels 60 to 74

As above, plus Feral Spirit and Spirit Walk (60), the tier 4 talent, Healing Rain (60), Capacitor Totem (63), Shamanistic Rage and
Healing Tide Totem (65), Fire Elemental Totem (66), Bloodlust and Heroism (70), Bind Elemental (72).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 25 | core | off cooldown | Feral Spirit (51533, 60): on cooldown (both guides) | yes: default 5.4 (`MS:63`) |
| 26 | good | the target is more than 8 yd away or I am rooted | Spirit Walk (58875, 60), Method opener | **dead (to check):** `CastSpiritWalkAction` makes the Spirit Wolf cast it (`ShamanActions.cpp:106-120`) and only if the wolf has the spell, `HasSpell(58875)`; in 5.4.8 it is the shaman's own spell (SpecializationSpells spec 263, level 60), so the wolf-cast version looks dead. The trigger waits for wolves and rearms every 32 s (`ShamanTriggers.cpp:308-331`) |
| 27 | good | mana below 40 percent, or health below 45 percent | Shamanistic Rage (30823, 65): free spells and -30 percent damage for 15 s | yes: 23 (`MS:113-117`); unknown below 65 (spell names below) |
| 28 | good | a hard fight | Fire Elemental Totem (2894, 66) in melee range | yes: `boost` 24 (`GS:254-256`), solo on every pull |
| 29 | good | a hard fight | Elemental Mastery (16166, 60) | yes: 29, whenever ready |
| 30 | good | health under 45 percent in a group fight | Astral Shift (108271, tier 1) | **missing** (gap 8) |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | tier 5 Ancestral Guidance; Hex as crowd control | talent edge at low health; no Hex line |
| 78 | Stormlash Totem on cooldown, with Bloodlust (both guides) | missing |
| 80 | Mastery: Enhanced Elements; Method: Mastery above Haste | passive; stat weights (see Stat order) |
| 81 | Unleash Elements on cooldown, Flame Shock while it runs (Method order) | yes: default 5.55; no Flame Shock pairing |
| 85 | Spiritwalker's Grace (not in the Enhancement lists) | n/a |
| 87 | Ascendance on cooldown, when Stormstrike is on cooldown (wowtbc) | `boost` 22.5 without the condition (`GS:258-259`) |
| 90 | Method single target: Lightning Shield, Windfury and Flametongue, Flame Shock, Fire Elemental, Stormlash, Feral Spirit, Elemental Mastery, Ascendance, Searing Totem, Stormstrike, Lightning Bolt at 5 stacks, Lava Lash, Unleash Elements, Flame Shock, Lightning Bolt | all there except Stormlash and the order of Lava Lash and Earth Shock; the default list asks for Elemental Blast first |

## Cooldowns and defensives

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Feral Spirit | 60 | on cooldown | on cooldown (30 s of two wolves per 2 min) | yes, default 5.4 |
| Elemental Mastery | talent 60 | on cooldown | on cooldown | yes, 29 |
| Fire Elemental Totem | 66 | on cooldown | boss, elite or hard pack | yes, `boost` |
| Ascendance | 87 | on cooldown | boss or elite | yes, `boost` |
| Stormlash Totem | 78 | on cooldown | with Bloodlust | missing |
| Bloodlust / Heroism | 70 | raid cooldown | boss or elite | yes, `boost` |
| Astral Shift | talent 15 | Method defensive | health under 45 percent in a group fight | missing |
| Shamanistic Rage | 65 | wowtbc defensive | mana under 40 percent, health under 45 percent | yes |
| Healing Tide Totem | 65 | wowtbc defensive | the group is hurt | self health only |
| Ancestral Guidance | talent 75 | both guides | members hurt | self health only |
| Wind Shear | 16 | not named | every interruptible cast of the pull target | yes |
| Purge | 12 | not named | a buff worth removing | yes |
| Ghost Wolf | 15 | Method: instant, a travel form | the group runs and the bot is not mounted | missing |
| Earthbind, Frost Shock | 26, 22 | not PvE | PvP and kiting only | no lines (PvP is future work) |

## Healer triage and mana (not a healer)

Healing that exists: Healing Surge on self at low health, and the out-of-combat "party member" heals (`NC:34-55`), which ask for
Riptide and Healing Wave first (Restoration only: unknown names for an Enhancement shaman) and then Healing Surge and Chain Heal.

Mana: the Enhancement shaman is cheap to run: instants and totems cost 25 percent and shocks 10 percent of the printed price
(Mental Quickness), melee hits restore mana (Primal Wisdom), Maelstrom Weapon takes 20 percent off a Nature spell per stack.
The expensive buttons are Chain Lightning (30.5 percent, only with stacks), Healing Surge (34.3 percent) and Purge (16.4
printed, 4.1 after the discount, instant); Healing Stream Totem costs 5.9 percent after the discount. The generic drink rule is
`AiPlayerbot.DungeonDrinkMana = 50` (`conf.dist:2272`).

## Spell names the Enhancement strategies ask for that an Enhancement shaman does not know

`SpellIdValue` logs `[SID] ... qualifier='<name>' resolved=0` whenever a name matches no known, non-passive spell
(`strategy/value/SpellIdValue.cpp`) and caches the answer 20 s per name and bot. From the code (inference, not checked against the
log), the names an Enhancement shaman of level 15 to 59 asks for and does not know:

| Name | Known from (Enhancement) | Asked from |
|------|--------------------------|------------|
| `elemental blast` | talent 90, only that pick (the premade takes Unleashed Fury) | default, first entry, every tick 15 to 89 and at 90 (`MS:61`) |
| `unleash elements` | 81 | default, every tick 15 to 80 (`MS:62`) |
| `stormstrike` | 26 | default, every tick 15 to 25 (`MS:63`) |
| `feral spirit` | 60 | default, every tick 15 to 59 (`MS:63`) |
| `fire nova` | 44 | "enemy within melee" (`GS:289-290`), every tick in melee at 15 to 43; "medium aoe" (`GS:284`) |
| `shamanistic rage` | 65 | "medium mana" and "low health" (`MS:113-117`) at 15 to 64 |
| `healing tide totem` | 65 | "low health" (`MS:122-123`) |
| `ancestral guidance` | talent 75 | "low health" (`MS:124-125`) at 15 to 74 |
| `fire elemental totem` | 66 | `boost` (`GS:254-256`), every pull when alone, 15 to 65 |
| `ascendance` | 87 | `boost` (`GS:258-259`) |
| `heroism`, `bloodlust` | 70 | `boost` (`GS:233-234`) |
| `ancestral swiftness` | talent 60, only that pick | `boost` (`GS:240-241`), every pull when alone |
| `windfury weapon main hand` | 30 | non-combat imbue (`NC:81-83`); the node falls through to Flametongue |
| `chain lightning` | 28 | only behind Maelstrom Weapon triggers (50 and up): no spam |
| `riptide`, `riptide on party`, `healing wave on party` | never (Restoration) | `NC:34-48` when a party member is hurt out of combat |
| `water walking` | 24 | `NC:31` when swimming, at 15 to 23 |
| `spirit walk` | 60 | gated by wolves standing: no spam |

Fix direction: give the default list, the `boost` edges, the imbue edges and the Fire Nova and Shamanistic Rage edges a
known-spell test, drop Elemental Blast from the default list unless the talent is picked, delete the Restoration names from the
non-combat heal list for non-healers, and the `resolved=0` count per bot should fall to near zero.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Stormstrike arrives at 26 (Primal Strike before it), Windfury at 30, Fire Nova at 44, Maelstrom
  Weapon at 50, Feral Spirit and Spirit Walk at 60, Fire Elemental at 66, Stormlash at 78, Unleash Elements at 81, Ascendance at 87;
  below those levels the lists above start without them. Before 50 an Enhancement shaman casts no Lightning Bolt in a rotation.
- Fire Nova needs a second enemy that carries Flame Shock (spell text); Method puts it first in every area fight.
- wowtbc's Maelstrom rule (Elemental Blast at 1 stack, Lightning Bolt at 2 or more) needs the Elemental Blast talent and care
  for the white swing; Method's 5 stacks is used.
- Enhancement is an Agility class with 2 attack power per Agility; Strength gives 1 (data), Intellect and Spell Power give
  almost nothing (Mental Quickness).
- Method lists tier 6 as Unleashed Fury or Primal Elementalist; the bots' default list assumes Elemental Blast.
- Lightning Bolt can be cast while moving in the 5.4.8 data (both guides say so; spell text and an InterruptFlags of 14 without the
  movement bit); the bots refuse every cast with a cast time while moving (`AI/PlayerbotAI.cpp:3850`, `:3922`). For Enhancement
  this matters only with fewer than 5 stacks, which the bots do not cast.
- Overgear, Icy Veins and Wowhead were not readable, so two guides were compared.

## Gap list (ordered)

Config (can go at once):

1. Talent link `AiPlayerbot.PremadeSpecLink.7.1` (`conf/playerbots.conf.dist:2824`) from `200010` to **`220010`** (Windwalk Totem at 30).
   The comment at `:2822-2823` needs the same change. Reason: Frozen Power is inert without a Frost Shock line. Tier 1 stays
   Astral Shift only if gap 8 is built.
2. Glyphs `PremadeSpecGlyph.7.1` (`:2825`): **no change**; the premade equals the standard list of both guides. The Spirit Walk
   glyph in the 75 slot starts to work when gap 7 is fixed.
3. Stat weights for the Enhancement (`Mgr/Item/StatsWeightCalculator.cpp:508-520`): Strength 0.7, Spell Power 0, Mastery 0 below
   level 80; keep Agility 1.4, Hit 2.1, Expertise 2.0, Haste 1.8, Critical 1.5. Check the hit cap value used for the overflow
   (`:131-132`).

Strategy (class fixes, wait for the owner's in-game test), ranked by value for levels 15 to 60:

4. **Unknown-name spam** (the table above): level and talent gates on the default list (`MS:60-65`: `stormstrike` from 26,
   `feral spirit` from 60, `unleash elements` from 81, `elemental blast` with the talent), known-spell tests on the Fire Nova,
   Shamanistic Rage, Healing Tide and Ancestral Guidance edges and the `boost` edges (`GS:231-261`), the Restoration names out of
   the non-combat heal list for non-healers (`NC:34-48`). Verify with the debug log: `resolved=0` lines per bot fall to near zero.
5. **Default order** (`MS:60-65`): Stormstrike (or Primal Strike before 26), then Lava Lash, then Earth Shock, i.e. Earth Shock to
   5.1; Fire Nova only with a second Flame Shocked enemy in reach (`ShamanActions.cpp:48-60`).
6. **Healing Stream Totem** (`TS:21-22`): remove the `no water totem` edge from the `totems` strategy for Enhancement and Elemental
   (keep it for Restoration). For Enhancement the cost is 5.9 percent per 30 s, for the Elemental 23.5.
7. **Spirit Walk**: cast the shaman's own Spirit Walk (58875) from a `CastBuffSpellAction` when rooted or slowed, or when the target
   is more than 10 yd away at the pull; replace the wolf-cast action (`ShamanActions.cpp:106-120`, trigger `ShamanTriggers.cpp:308-331`).
   To check live first that the wolf does not carry the spell.
8. **Astral Shift** (talent 15): a new action and a `low health` edge (the pattern of `CastAncestralGuidanceAction`, `SA:621-626`); or
   change the tier 1 pick to Nature's Guardian (question 1).
9. **Magma Totem** at 3 or more attackers within 8 yd (derived): a trigger on "medium aoe" that casts Magma Totem when the fire
   slot holds only Searing Totem or is empty; keep Searing as the single target totem (`TS:26-28`).
10. **Searing Totem lifetime**: `needLifeTime` 10 s (`SA:261-266`), so a 3 s pull does not buy a totem.
11. **Healing Surge with Maelstrom Weapon** (derived, good tier): at 3 or more stacks and health under 70 percent, alone or without a
    healer in the group, cast Healing Surge before spending the stacks on Lightning Bolt (new trigger, or a health test on the
    existing edges at `MS:94-98`).
12. **Ghost Wolf** (15) as a travel form: no code exists for it (only the removal on mounting, `AI/PlayerbotAI.cpp:3550`); low value.
13. **Flame Shock refresh** (expert): `beforeDuration` of about 3 s on `FlameShockTrigger`, and Lava Lash on cooldown already spreads
    it (60103); a Flame Shock on a second target is not needed for Enhancement.
14. Stormlash Totem (78), Tremor Totem (54), Capacitor Totem (63), Hex (75): later, with the 61 to 90 pass.
15. Skill tiers: the Enhancement strategies have no tier gating (`BotSkill` is used in one shaman trigger only, the Restoration
    filler, `ShamanTriggers.cpp:503`). Lines marked good and expert above (Searing Totem lifetime, Magma Totem, Fire Nova pairing,
    the Maelstrom heal, Purge, Cleanse Spirit, Astral Shift, the Flame Shock refresh) are the ones a casual bot should drop.
16. To check live: the Spirit Walk wolf cast; Fire Nova against one enemy; the Windfury main-hand fallback before 30; a dual wield
    imbue with no off-hand weapon; Stormstrike swap of `primal strike` at 26; Chain Heal and Healing Rain for a non-Restoration
    shaman; Lava Lash and Stormstrike with no off-hand weapon; `[SID]` counts per bot.

Owner questions:

1. Tier 1: Astral Shift (guide; needs gap 8, two lines of code) or Nature's Guardian (a passive that works with no code). Tier 2:
   Windwalk Totem (guide, inert) or Earthgrab Totem (inert too). Tier 60: Elemental Mastery (a line exists, Method single target)
   or Echo of the Elements (Method's dungeon build, a passive that needs no timing).
2. Tier 90: Unleashed Fury (Method) or Elemental Blast (wowtbc's rotation, the bots' default list, a line that exists).
3. Healing Stream Totem for damage specialisations: drop it, or keep it for a healer-less group.
4. Maelstrom Weapon: Lightning Bolt at 5 stacks only (Method) or also at 4 (today).
5. Magma Totem at 3 attackers (derived, cheap for Enhancement) or at more than 5 (Method, a raid number).

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 3 players of this specialisation (rating 2425 to 2556). 2v2: no player of this specialisation. Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** none, only 3 players in all (noted, not changed)
- **BT-324 result:** open (3 players)
- **Set now:** `PremadeSpecLinkPvp.7.1 = 020112`, `PremadeSpecGlyphPvp.7.1 = 71155,43388,45771,43381,43385,41531`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Nature's Guardian 100% / - / 3 of 3 | Nature's Guardian | Nature's Guardian | matches |
| 2 (30) | Windwalk Totem 100% / - / 3 of 3 | Windwalk Totem | Windwalk Totem | matches |
| 3 (45) | Call of the Elements 100% / - / 3 of 3 | Call of the Elements | Call of the Elements | matches |
| 4 (60) | Ancestral Swiftness 100% / - / 3 of 3 | Ancestral Swiftness | Ancestral Swiftness | matches |
| 5 (75) | Ancestral Guidance 100% / - / 3 of 3 | Ancestral Guidance | Ancestral Guidance | matches |
| 6 (90) | Elemental Blast 100% / - / 3 of 3 | Elemental Blast | Elemental Blast | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Feral Spirit (45771) | 100% | - | 3 of 3 | yes | yes |
| major | Lightning Shield (71155) | 100% | - | 3 of 3 | yes | yes |
| major | Totemic Vigor (45778) | 67% | - | 2 of 3 | no | no |
| major | Healing Storm (43344) | 33% | - | 1 of 3 | no | no |
| major | Flame Shock (41531) | 0% | - | 0 of 3 | yes | yes |
| minor | the Lakestrider (41537) | 100% | - | 3 of 3 | no | no |
| minor | Lava Lash (41540) | 100% | - | 3 of 3 | no | no |
| minor | Rain of Frogs (104134) | 67% | - | 2 of 3 | no | no |
| minor | Totemic Encirclement (43388) | 33% | - | 1 of 3 | yes | yes |
| minor | Astral Recall (43381) | 0% | - | 0 of 3 | yes | yes |
| minor | Far Sight (43385) | 0% | - | 0 of 3 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **Open (BT-451):** three players in 3v3, none in 2v2: all six tiers match the set. Glyphs: Feral Spirit and Lightning Shield (100) are held; Totemic Vigor 67 is not (the set holds Flame Shock, 0 percent).
