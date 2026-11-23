# Damage class sheets: what is applied (2026-11-17)

The 23 damage sheets were implemented on 2026-11-11 and 2026-11-16 (owner approval of the gap lists 2026-11-11, tiers
2026-11-16). Their "Bots today" columns and unmarked gap lists still describe the code before that work, so this page is
the current state. Nothing below is live-verified yet: the first column of proof is the `[DPSFIGHT]` line (see
"Measurement"). A line is **read** when a call site was read, **data** when it was checked against the 5.4.8 tables,
**inference** when it follows from the code only.

## Checks done on 2026-11-17

| Check | Result |
|-------|--------|
| 865 spell references of the 23 sheets ("Name (id, level)") against `Spell.dbc` and `SpellLevels.dbc` | no wrong id; the 57 hits of the script were glyph effect rows and file line numbers (data) |
| Every trigger and action name used by the class strategies is registered in a context | 0 unregistered (read) |
| Every spell name in `DpsSheetPolicy.h` tier tables is a 5.4.8 spell name and is used by an action | all 23 tables (read) |
| Active spells of each damage specialisation up to level 60 that no strategy names | none of value; unused: Astral Communion (balance, 20), Spinning Fire Blossom (windwalker, 48), Soul Harvest (affliction, 27), Demonic Leap (demonology, 12), Wound and Leeching Poison, Slow Fall, Snake Trap, Ice Trap (data, grep) |
| The tier gate (`DpsSheetPolicy`) against the sheets | defensives, area lines and cooldowns are "good"; duties are never gated (read) |

## Who plays which damage specialisation (inference)

330 random bots on 2026-11-17 (`RNDBOT%` accounts): 1 to 9: 64, 10 to 19: 112, 20 to 35: 82, 36 to 60: 69,
61 and up: 3. Specialisations are rolled from `AiPlayerbot.RandomClassSpecProb`; the expected damage dealers by band:

| Band | Most bots (expected count) |
|------|-----------------------------|
| 36 to 60 (45 of 69 are damage) | frost death knight 11, unholy death knight 8, then 1 to 2 of every other specialisation (27 of the 69 are death knights at 55 to 60) |
| 10 to 35 (123 damage) | frost mage 8, fury warrior 8, windwalker 7, assassination and combat rogue 7 each, retribution 7, elemental and enhancement shaman 7 each, three hunters 6 each, arcane and fire mage 6 each, three warlocks 6 each, shadow 5.5, arms 4, balance and feral 3 each, subtlety 1.6 |

The finder fills dungeons at 10 to 35 far more often than at 36 to 60 today, so those specialisations carry the first
proof runs; the death knight pair carries the owner's band.

## Status per specialisation

"Applied" names the commit group; every one is on `main` with a compile and live check pending in the ledger unless
stated. "Open" lists only items that are open after reading the code (not the stale sheet gap list).

| Specialisation | Applied | Open (ranked) |
|----------------|---------|---------------|
| Arms warrior | `5927134`, BT-261, BT-260 tiers | Recklessness gate (owner decision), Commanding Shout |
| Fury warrior | `5927134`, BT-260 | level 81 and up Colossus Smash window (outside bands), Sunder Armor rank |
| Retribution paladin | `5ae1c32`, BT-260 | mana plan (needs a live measurement) |
| Beast mastery, marksmanship, survival hunter | `8891ef5`, BT-340, BT-260 | Snake Trap and Ice Trap for Lock and Load (survival), pet spells by autocast (live check) |
| Assassination, combat, subtlety rogue | `10fb0b2`, BT-341, BT-260 | Leeching Poison, Redirect and Crimson Tempest (75 and up), Vanish at two attackers (owner decision) |
| Shadow priest | `7f3a3be`, BT-265, BT-260 | none known |
| Frost, unholy death knight | `65b8df3`, BT-342, BT-260 | Blood Tap line (75 and up), remaining level gates; unholy: disease refresh by Plague Strike from 68 (sheet item 7, not read) |
| Elemental, enhancement shaman | `b5ed934`, BT-260 | Hex edge (75, outside the bands) |
| Arcane, fire, frost mage | `359eb60`, BT-264, BT-322, BT-260 | Ice Floes (dead tier 15 talent in the premade) |
| Affliction, demonology, destruction warlock | `13d3b66`, BT-159, BT-262, BT-260, this run's DoT refresh | Flames of Xoroth, Grimoire of Service, Havoc before Chaos Bolt (expert) |
| Windwalker monk | `2b77b34`, BT-263, BT-260 | Spinning Fire Blossom (low value) |
| Balance, feral druid | `1a8d404`, `d6f7a10`, BT-260 | Astral Communion (balance, 20), Incarnation (60 and up, premade picks none), Wild Mushroom (84) |

Group jobs of the damage role (role sheet `docs/role-sheets/dps.md`): target order BT-161, threat gate BT-161, interrupts
BT-164, area spells next to crowd control BT-165, hidden attacker walk BT-190, off-heals BT-161, cooldown gate BT-161
(fixed for resurrections and Innervate on 2026-11-17).

## Measurement

Damage dealers in a group write a `[DPSFIGHT]` line when a fight of 8 s or more ends (`src/strategy/generic/DpsMeter.cpp`).
After a proof run: `docker logs <worldserver container> 2>&1 | python3 -I tools/dps_by_spec.py --min-level 10`.

| Change | What to read |
|--------|--------------|
| Skill tiers (BT-260) | the tier table of the script: median dps expert above good above casual per specialisation (the same level) |
| Cooldown rules (BT-161, BT-262, BT-264, BT-341, BT-342) | the cooldown table: each spell cast in boss and elite pack fights, almost never on trash |
| Idle damage dealers | the `idle` column: fights of 8 s or more with no damage |
| Expert DoT refresh (this run) | median dps of expert against good warlocks of one specialisation, level band and fight weight |
| Resurrection and Innervate gate (this run) | a dead member in a fight with one elite or none, then the druid's Rebirth or the death knight's Raise Ally (`[CAST]` at debug level, or the member alive again) |

The damage is the hit before absorbs and overkill (core damage hook), so compare specialisations of the same level on the
same packs; it does not match a combat log.
