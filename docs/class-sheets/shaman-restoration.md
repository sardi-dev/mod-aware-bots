# Shaman: Restoration (healer)

- **Status:** checked against 5.4.8, pilot sheet (2026-11-10). Implemented, compile and live check pending: gap list items 1 to 3
  (owner approved 2026-11-10). The totems strategy is on for the specialisation, so item 2 was a low priority, not a missing totem.
- **Sources:** Method, "Restoration Shaman Class Guide - MoP Classic" (Phase 1, level 90 raids and dungeons), read
  2026-11-10; the server's own data files (Spell.dbc, SpellLevels.dbc, SpecializationSpells.dbc, Talent.dbc of build
  18414) for every spell id, learn level and talent tier. No levelling guide was readable, so the level bands below
  are the level 90 priority cut down to what the specialisation knows at each level.
- **Levels covered:** 15 to 90, with the detail on 15 to 60.

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.7.2 = 221121` (one digit per
tier, the column counted from 0).

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today |
|--------------|------------|----------------------|----------|------------|
| 1 (15) | not named by the guide | | | Astral Shift (108271) |
| 2 (30) | Windwalk Totem | Earthgrab Totem when a mass root is needed | 108273 | Windwalk Totem, same |
| 3 (45) | Call of the Elements | Totemic Projection on fights with much movement | 108285 | **Totemic Persistence (108284), differs** |
| 4 (60) | Ancestral Swiftness | Elemental Mastery as a raid healing cooldown | 16188 | Ancestral Swiftness, same |
| 5 (75) | Conductivity in dungeons | Rushing Streams in raids | 108282 | Conductivity, same |
| 6 (90) | Primal Elementalist | | 117013 | Primal Elementalist, same |

## Glyphs

| Type | Guide | Bots today (PremadeSpecGlyph.7.2) |
|------|-------|-----------------------------------|
| Major | Telluric Currents, Healing Stream Totem; also Chaining, Spiritwalker's Grace | Telluric Currents, Spiritwalker's Grace, Healing Stream Totem (per the comment in the config) |
| Minor | Lakestrider | Deluge, Astral Recall, Far Sight |

## Stat order

Intellect, then Spirit, then Haste to the first breakpoint, then Critical Strike, then Mastery. For loot rolls and
quest rewards below level 60 this means: Intellect mail (cloth or leather before level 50, when Mail Specialization
arrives), Spirit over the other secondary stats.

## What the specialisation knows, by level (5.4.8 data)

| Level | Spell (id) |
|------:|------------|
| 7 | Healing Surge (8004) |
| 10 | Riptide (61295), with the specialisation |
| 14 | Ancestral Spirit (2008) |
| 16 | Wind Shear (57994) |
| 18 | Purify Spirit (77130) |
| 20 | Healing Wave (331), Water Shield (52127) |
| 26 | Earth Shield (974) |
| 30 | Earthliving Weapon (51730), Healing Stream Totem (5394) |
| 44 | Chain Heal (1064) |
| 50 | Tidal Waves (51564, passive) |
| 56 | Mana Tide Totem (16190) |
| 58 | Earth Elemental Totem (2062) |
| 60 | Greater Healing Wave (77472), Healing Rain (73920) |
| 65 | Healing Tide Totem (108280) |
| 66 | Fire Elemental Totem (2894) |
| 70 | Spirit Link Totem (98008) |
| 81 | Unleash Elements (73680) |
| 85 | Spiritwalker's Grace (79206) |
| 87 | Ascendance (114052) |

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only).

### Levels 15 to 29
| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 1 | core | from 20: no Water Shield | Water Shield | yes (`HealShamanStrategy.cpp:130`, out of combat `ShamanNonCombatStrategy.cpp:96`) |
| 2 | core | from 26: the tank has no Earth Shield | Earth Shield | yes (`HealShamanStrategy.cpp:122`) |
| 3 | core | a member is hurt and has no Riptide | Riptide | yes |
| 4 | core | a member is hurt | Healing Wave from 20, Healing Surge before that and in emergencies | yes: Healing Wave is tried first and is simply not castable before 20 |
| 5 | good | a harmful magic effect or curse on a member, from 18 | Purify Spirit | yes |
| 6 | good | an enemy casts, from 16 | Wind Shear | to check: no line in the heal strategy itself |
| 7 | good | nobody needs healing | Lightning Bolt (mana back with the Telluric Currents glyph, level 25) | **gap: the heal strategy never casts it, so the glyph the bots carry does nothing** |

### Levels 30 to 44
As above, plus:
| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 8 | core | weapon without imbue | Earthliving Weapon | yes, out of combat only (`ShamanNonCombatStrategy.cpp:93`) |
| 9 | core | in a fight, off cooldown | Healing Stream Totem | **to check: the line in the heal strategy is commented out; only the optional totems strategy places it** |

### Levels 45 to 59
As above, plus:
| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 10 | core | three or more members hurt, from 44 | Chain Heal, on a member with Riptide | yes for Chain Heal; the Riptide target preference is not coded (expert) |
| 11 | good | mana low, from 56 | Mana Tide Totem | yes, already at medium mana |
| 12 | expert | from 50: two Tidal Waves charges | spend them on Healing Wave (light damage) or Greater Healing Wave (heavy) | gap: Tidal Waves is not read anywhere |

### Level 60 and above
| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 13 | core | the group takes damage together, from 60 | Healing Rain | yes |
| 14 | core | heavy damage on one member, from 60 | Greater Healing Wave | yes |
| 15 | good | high group damage, from 65 | Healing Tide Totem | **gap in the heal strategy** (the action exists elsewhere in the class) |
| 16 | good | high group damage, from 70 | Spirit Link Totem | **gap: not in the module** |
| 17 | good | must move while healing, from 85 | Spiritwalker's Grace | **gap: not in the module** |
| 18 | good | from 81, before Healing Rain | Unleash Elements | partly: cast on a hurt member, not paired with Healing Rain |
| 19 | expert | from 87, high damage | Ascendance | yes |

## Differences from the guide (5.4.8 against MoP Classic)

- The guide is for level 90. Its Stormlash, Earth and Fire Elemental Totem lines are raid damage help and are left out
  of the dungeon lists below 90.
- World data: the trainer table lists Healing Rain at level 83, the spell data says 60. To check which one the server
  applies when a character levels (a server data issue, not a bot issue).

## Gap list (ordered by value for levels 15 to 60)

1. Lightning Bolt as the idle filler for healers with Telluric Currents (mana for free; today the glyph is wasted).
2. Healing Stream Totem on cooldown in fights from level 30 (verify what the totems strategy does first).
3. Tier 3 talent: Call of the Elements instead of Totemic Persistence (config only).
4. Wind Shear by the healer when nobody else interrupts (verify first).
5. Later levels: Healing Tide Totem (65), Spirit Link Totem (70), Spiritwalker's Grace (85), Unleash Elements before
   Healing Rain (81), Tidal Waves spending (expert).

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 4 players of this specialisation (rating 2456 to 2721). 2v2: 1 player (rating 2741 to 2741). Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** pooled (5 players)
- **BT-324 result:** updated (BT-453, pooled 5 players)
- **Set now:** `PremadeSpecLinkPvp.7.2 = 020100`, `PremadeSpecGlyphPvp.7.2 = 45778,43388,43725,45775,43385,45777` (was `45778,43388,43725,43381,43385,41533`)

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Nature's Guardian 100% / 100% / 5 of 5 | Nature's Guardian | Nature's Guardian | matches |
| 2 (30) | Windwalk Totem 50% / 100% / 3 of 5, Earthgrab Totem 50% / 0% / 2 of 5 | Windwalk Totem | Windwalk Totem | matches |
| 3 (45) | Call of the Elements 75% / 0% / 3 of 5, Totemic Projection 25% / 100% / 2 of 5 | Call of the Elements | Call of the Elements | matches |
| 4 (60) | Ancestral Swiftness 100% / 100% / 5 of 5 | Ancestral Swiftness | Ancestral Swiftness | matches |
| 5 (75) | Rushing Streams 100% / 100% / 5 of 5 | Rushing Streams | Rushing Streams | matches |
| 6 (90) | Unleashed Fury 100% / 100% / 5 of 5 | Unleashed Fury | Unleashed Fury | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Ghost Wolf (43725) | 100% | 100% | 5 of 5 | yes | yes |
| major | Totemic Vigor (45778) | 100% | 100% | 5 of 5 | yes | yes |
| major | Hex (45777) | 50% | 100% | 3 of 5 | no | yes |
| major | Healing Stream Totem (41533) | 50% | 0% | 2 of 5 | yes | no |
| minor | Totemic Encirclement (43388) | 100% | 100% | 5 of 5 | yes | yes |
| minor | Deluge (45775) | 50% | 100% | 3 of 5 | no | yes |
| minor | the Spectral Wolf (43386) | 25% | 100% | 2 of 5 | no | no |
| minor | Astral Recall (43381) | 0% | 0% | 0 of 5 | yes | no |
| minor | Far Sight (43385) | 0% | 0% | 0 of 5 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **What changed (pooled 4 + 1 = 5):** the major glyph Hex (3 of 5; on 5.4.8 it shortens the Hex cooldown, and the bots have a Hex action) replaces Healing Stream Totem (2 of 5), and the cosmetic minor Deluge (3 of 5) replaces Astral Recall (0). All six tiers already matched (tier 30 Windwalk Totem and tier 45 Call of the Elements each 3 of 5, exactly 60 percent). Ghost Wolf, Totemic Vigor and Totemic Encirclement are 5 of 5.
- **Caution:** this is the thinnest basis of any change: five players, the Hex and Deluge picks and two tiers sit at exactly 60 percent, and the single 2v2 player decides Hex and Deluge. 3v3 alone (4 players) was only noted.
