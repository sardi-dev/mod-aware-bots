# 1v1: duels, open-world fights and isolated battleground skirmishes

Status (2026-11-18): round 1 and 1b are live (build 13, measured on build 13 and 14); round 2 (this file, "Round 2" below)
is on `feat/duel-r2`, not yet built. Baselines are in the tables at the end.

## What the owner wants (2026-11-17)

- Ranged bots (hunter, mage, warlock, caster druid, shadow priest, elemental shaman) keep their distance from a melee
  enemy: slow or root it, then move away (Disengage, Blink, roots, line of sight); they never stand in melee range of a
  melee class without a reason. Melee bots chain gap closers and slows and stay on the target.
- By skill tier: expert kites and chases cleanly with good timing, good kites with slows but makes some mistakes, casual
  keeps some distance and often lets the melee catch up. Reaction limits: expert 250 to 350 ms, good 350 to 500 ms, casual
  600 to 900 ms. No information a player would not have.
- One engine for duels, open-world 1v1 and isolated battleground skirmishes. Arena team play stays with the arena work.
- Measured with fixed matchups (levels 20 and 60), position traces, and samples of the random bots' own duels at the duel
  spots.

## How it is measured

The duel bench (a small driver script outside this repository, GM commands over the worldserver console only):

- Lane: GM Island (map 1, 16226.2 16257.0 13.2), no creatures and no other bots. Each pair is teleported there 9 yd apart
  (the duel request reaches 10), revived, set to full health and power, and `.botscenario <A> startduel req <B>` starts the
  duel. Two duels per pair; the pair goes back to its saved position afterwards.
- Data: `[PROBE]` rows every 500 ms for both bots (position, health, movement, duel state), a spell watch on the slows,
  roots, stuns, fears, gap closers, escapes and defensives of the bench classes (`[SPELLWATCH] cast` lines), `[DUEL]
  START/END`. The playerbots logger is raised to debug for the run only (runtime, no config file) and put back to info.
- Metrics per duel: winner, duration, share of time the two are within 8 yd (for a ranged side against a melee side: the
  time it stood in melee), mean distance, the time the melee side needed to come back within 8 yd after each separation
  beyond 10 yd, share of time the first bot was moving, and the casts per kind. Traces are CSV, two rows per second
  (positions interpolated between probe rows).
- The random bots' own duels: a second script watches the duel spot requests at Goldshire and Razor Hill and probes both
  duellists until the duel ends; same metrics.

## Baseline (live build 10, 2026-11-18, before round 1)

See the tables at the end of this file. In short: a ranged bot against a melee bot stood within 8 yd for 95 to 100 percent
of the duel unless it was a mage with Blink; it moved in 0 to 15 percent of the samples; no hunter cast Concussive Shot, no
warlock cast Fear or Curse of Exhaustion, no warrior cast Hamstring or Charge.

## Why the bots did not kite or chase

1. **Nothing slowed the melee.** Every "snare target" trigger (Concussive Shot, Hamstring, Frost Shock, Chains of Ice and
   the rest, `SnareTargetValue`) only picks an attacker whose motion generator is a chase. A player never has one (the
   client moves it) and a bot chasing by point splines has none either (`mgen=0` in every probe row of the bench), so the
   slows never fired in a duel. A melee bot also never slows a target that hits it back (the same value skips it).
2. **The old kite waited for a slow that never came.** `pvp kite` (BT-145) only backs off from an enemy that is already
   slowed or held, so with point 1 it almost never ran; it also had no tier timing.
3. **Disengage did not move the bot.** The bot side of the knockback packet called `MotionMaster::MoveKnockbackFrom`, which
   returns at once for a player on this core. The level 20 hunter cast Disengage seven times in one duel and its position
   never changed. Every other knockback (Thunderstorm, Typhoon, boss knockbacks) had the same dead end.
4. **The melee circled instead of swinging.** "set behind" (ACTION_MOVE + 7, above every spell) walked the warrior around a
   standing hunter: a player turns, so the bot kept walking.
5. **Class escapes fired without tier timing.** "disengage pvp" and "mage player nova" fire on any player in reach, for a
   casual bot as fast as for an expert, and Blink had no duel-area check (the level 60 mage blinked out of the duel area
   and lost by fleeing).
6. Warlocks had no player crowd control in their duel rotation at all at level 20 (no Fear, no Curse of Exhaustion cast).

## Round 1 (branch `feat/duel-kiting`)

- `src/Utils/OneVsOnePolicy.h`: pure, tested (`tests/test_one_vs_one.cpp`). Facts in (distance, closing or opening, the
  enemy's speed against the bot's, held or slowed on either side, how long the melee has been near, in contact or apart,
  which kit spells are ready, room to back off), a scored option list out:

  | Side | Option (score) | When |
  |------|----------------|------|
  | ranged | escape (900) | caught (within melee reach + 1.5 yd) by a free melee enemy, escape ready, reaction time passed, not a slipping window; casual only at 60 percent health or less |
  | ranged | root (850) | a free melee enemy inside the root's reach, caught or coming in |
  | ranged | step back (800) | the melee is slowed or held and inside the keep distance less 7 yd; never while casting or held, never out of the duel area |
  | ranged | slow (700) | a free melee enemy coming in inside the tier's threat radius (or already on the bot) |
  | ranged | hold (100) | everything else, and always against a ranged enemy: the rotation runs |
  | melee | gap closer (900) | gap open (reach + 2 yd) inside the closer's range band, reaction time after the separation |
  | melee | freedom (750) | slowed or rooted while the gap is open |
  | melee | slow (700) | a ranged enemy, or a melee enemy moving away, not slowed yet, inside the slow's reach |
  | melee | speed (600) | the gap stayed open 2.5 s past the reaction time |
  | melee | close (300) / hold (100) | the rotation's reach and swings |

- Tiers:

  | Tier | Reaction (fixed per bot) | Slip windows (3 s) | Threat radius | Keep distance | Escape |
  |------|--------------------------|--------------------|---------------|---------------|--------|
  | expert | 250 to 350 ms | 0 percent | 25 yd | 25 yd | whenever caught |
  | good | 350 to 500 ms | 20 percent (no step back, no escape in that window) | 20 yd | 20 yd | whenever caught |
  | casual | 600 to 900 ms | 50 percent (also no slow, no gap closer, no melee slow) | 10 yd | 15 yd | at 60 percent health or less |

- Class kits (data in the same header, ids checked against the 5.4.8 `Spell.dbc` names): warrior Charge 100, Heroic Leap
  6544, Hamstring 1715, Piercing Howl 12323; paladin Hand of Freedom 1044, Speed of Light 85499; hunter Concussive Shot
  5116, Wyvern Sting 19386, Scatter Shot 19503, Binding Shot 109248, Disengage 781; rogue Shadowstep 36554, Burst of Speed
  108212, Sprint 2983; priest Psychic Scream 8122, Void Tendrils 108920, Psychic Horror 64044; death knight Death Grip
  49576, Chains of Ice 45524, Death's Advance 96268; shaman Frost Shock 8056, Earthbind Totem 2484, Earthgrab Totem 51485,
  Thunderstorm 51490; mage Frost Nova 122, Frostjaw 102051, Cone of Cold 120, Frostbolt 116, Blink 1953; warlock Mortal Coil
  6789, Howl of Terror 5484, Fear 5782, Curse of Exhaustion 18223, Demonic Circle: Teleport 48020; monk Flying Serpent Kick
  101545, Chi Torpedo 115008, Roll 109132, Clash 122057, Disable 116095, Tiger's Lust 116841; druid Wild Charge 102401,
  Entangling Roots 339, Mass Entanglement 102359, Faerie Swarm 102355, Displacer Beast 102280, Dash 1850. A row is used only
  when the bot knows the spell, its cooldown is ready and the core accepts the cast.
- `src/strategy/generic/OneVsOne.{h,cpp}` (facts, tracking, spell pick), strategy, trigger and action `1v1` (ACTION_MOVE +
  7) in every bot's combat engine. Who counts as the lone enemy: the started duel's opponent; otherwise a hostile player
  the bot targets with no other enemy player within 30 yd (open world and battlegrounds). Never in an arena, mounted, while
  carrying a battleground objective, or for a healer in a group. `AiPlayerbot.OneVsOne.Enable = 0` restores the old kite.
- While the engine owns a fight, "disengage pvp", "mage player nova" and "set behind" stand down.
- Knockbacks move bots (`PlayerbotAI` knockback handler builds the arc and launches it with `MoveJump`).
- A `[1V1]` debug line per decision change: `[1V1] <bot> vs <enemy> choice= why= spell= dist= tier= self= enemy= slowed=
  held=`.

## Not in round 1 (done in round 2 unless marked)

- Line of sight play for ranged (pillars, trees): built in round 2 (cover); the bench lane is open sand, so it is not
  measured there.
- Defensives and trinkets by tier in a 1v1: built in round 2. Interrupt timing and crowd control diminishing returns stay open.
- Crippling Poison upkeep for rogues (a weapon buff, not a cast), Seal of Justice, Infected Wounds and other passive slows.
- The decision layer (design decision 0014) picking among the scored options.
- Strafing and backpedalling while casting instants (the bot turns and runs; a player backpedals at reduced speed).

## Re-measure after the build

Same pairs (the same ten bots, five pairs at level 20 and five at level 60), same lane, same metrics, two duels per pair,
plus a new natural sample at the duel spots, only while no human is online.

Pass marks for round 1: a ranged bot against a melee bot within 8 yd for at most 50 percent of the duel at expert and good
tier (casual may stay higher); at least one slow or root per duel from hunters and warlocks; the hunter's position changes
by 15 yd or more after each Disengage; melee gap closers used after separations; no duel lost by fleeing the area.

## Round 2 (branch `feat/duel-r2`, 2026-11-18)

### What the build 14 bench showed (before the fixes)

Same ten bots as round 1b (one swap: Oldes was offline, Reamme took the level 20 warrior's place), two duels per pair.
Raw rows of the bench runs b14-20 and b14-60 are not published. The table is at the end of this file.

1. **Warlock against paladin, 82 to 98 percent within 8 yd.** Three causes in the live log. (a) At level 20 the paladin was
   read as a caster (`enemy=ranged`): the spec test calls a holy paladin ranged, so the warlock held its place. (b) At level
   60 the paladin runs at 1.25 to 1.3 times the warlock's speed (Pursuit of Justice) and Curse of Exhaustion only brings it to
   0.91, so it never counts as slowed (the rule is under 0.85) and the warlock gains 0.6 yd/s at best: against that paladin
   only the stuns and fears (Mortal Coil, Howl of Terror) buy distance. (c) While the paladin was held the warlock was often
   in the middle of a Shadow Bolt (`casting=1`, no step back), and after two steps the straight line back ran out of the duel
   area (`room=0` in 23 decisions of one duel) so it stood against the boundary.
2. **Mage against rogue, the rogue never reached the mage (131 s, 92 Burst of Speed or Sprint casts).** Not pathing, not line
   of sight: the rogue walked to 15.7 yd from the mage, then the engine's SPEED option fired every tick (`ready=----V-`, Burst
   of Speed has no cooldown and the spell is accepted while it runs); the engine's own action (ACTION_MOVE + 7) ends the tick
   when its cast succeeds, so the walk to the mage (lower priority) never ran. 11 casts in the second duel of that pair, 92 in
   the first. Rogues stand still in stealth for a while at the start as well (their own opener), which looks the same on a
   trace.
3. **Floresa, two daggers.** `CanEquipWeapon` rejects melee weapons for hunters, so the factory never chose them. The path is
   `ItemUsageValue`: an empty main hand answers any weapon with "bad equip" and the equip action takes it; the incremental
   equipment pass then kept the dagger because the bow had to score 1.2 times the dagger's. 6 of the 30 random hunters held a
   melee weapon (levels 14 to 34).

### What round 2 changes

- Roles by class (`RoleOfClass`): warriors, rogues, death knights, paladins and monks are melee in every spec; a hunter is
  ranged only with a bow, gun or crossbow in hand; the rest follow the spec.
- A good or expert ranged bot steps away from a caught melee enemy in the middle of a cast (casual bots finish it).
- The step back is a point (`FindBackOffPoint`): nine headings from straight away to 120 degrees either side, walkable in a
  straight line, inside the duel area less 1.5 yd, gaining at least 2 yd on the enemy; the farthest wins. Used for `STEP_BACK`.
- No slow on an enemy that already carries a slow debuff (8 Curse of Exhaustion per duel on the bench, each in place of a spell).
- No speed spell while the bot is 15 percent faster than the enemy, and not again within 3 s of the last cast.
- `BotFactory::EnsureHunterRangedWeapon` and `ItemUsageValue` never offering a melee weapon to a hunter.
- New options (scores in the order they compete, highest first):

  | Option (score) | Side | When |
  |----------------|------|------|
  | cc-break (950) | both | stunned, feared, rooted or asleep for the tier's reaction time, a breaker ready (racial, class spell, PvP trinket), not in a slipping window, enemy within 40 yd |
  | defensive (880) | both | health at or under the tier line, for the reaction time, enemy not held, enemy inside melee reach + 5.5 yd (melee) or 40 yd (caster), not in a slipping window |
  | cover-hold (830) | ranged vs caster | taking cover, no line of sight, enemy beyond 8 yd, under 6 s since the cover started |
  | cover (820) | ranged vs caster | health at or under the tier's cover line for the reaction time, a cover point found, not casting, not in a slipping window |

  Tier lines: defensive 55, 40, 25 percent (expert, good, casual); cover 65, 50, never. The cover window is 6 s, the next one
  starts 15 s after the first; the point is the nearest on rings of 5, 8 and 11 yd that is out of the enemy's line of sight
  (rays at eye height, 2 yd), walkable in a straight line and inside the duel area.
- Defensives per class (kit rows, tried in order, a row is used only when known, off cooldown, not already running and, for
  Evasion, Cloak of Shadows and Anti-Magic Shell, when it fits the enemy's kind): warrior Die by the Sword, Shield Wall,
  Enraged Regeneration; paladin Divine Protection, Divine Shield; hunter Deterrence; rogue Evasion, Cloak of Shadows; priest
  Dispersion, Desperate Prayer, Power Word: Shield; death knight Icebound Fortitude, Anti-Magic Shell, Vampiric Blood; shaman
  Astral Shift; mage Ice Barrier, Ice Block; warlock Unending Resolve, Dark Regeneration, Sacrificial Pact; monk Dampen Harm,
  Fortifying Brew; druid Barkskin, Survival Instincts. Breakers: Every Man for Himself, Will of the Forsaken, Escape Artist (all
  classes, by race), Berserker Rage (warrior), Lichborne (death knight), the PvP trinket (item spell 42292) when one is equipped.
  The racials strategy's three triggers (loss of control, fear charm sleep, movement impaired) wait for the same reaction time
  while a 1v1 owns the fight; the class rotations keep their own last-ditch defensives at their own lines.
- The `[1V1]` line grew: `ready=` has eight letters (slow, root, escape, closer, speed, freedom, defensive, breaker), `cover=`
  is 0 to 3 (1 a point is known, 2 no line of sight now, 3 both), `hp=`.

### Defensives and breaks after the shared rules (BT-522)

`defensiveReady` and `ccBreakReady` are now the verdict of the shared PvP rules (docs/pvp/awareness-r1-r2.md): a smaller defensive
early in the enemy's go and the immunity last, one per go, the cheapest break before the medallion and the medallion by role.
The tier health lines of the table above (55, 40, 25) apply only with `AiPlayerbot.Pvp.Defensives.Enable = 0`. The `[1V1]` line
shows `ready=...D...B` as before; `choice=defensive` and `choice=cc-break` now appear at the rules' moments (a go at 70 to 80
percent for an expert, not at a fixed health).

### Not done

- The cover rule has no live measurement: the bench lane is open sand. A lane with a pillar or a wall is needed (the GM
  Island building is 20 yd north of the lane, untried).
- Strafing and backpedalling while casting instants, crowd control diminishing returns, interrupt timing, interrupting a
  fear with a trinket on the enemy side, using the enemy's casting state for cover.
- Fast melee enemies (Pursuit of Justice, Dash, Sprint) cannot be kited by a slow alone; only stuns, fears and roots buy the
  distance, and the engine already uses them first.

### Re-measure after the build (round 2)

Same pairs and lane as round 1b (`tools/bots-bench/duel/duel_bench.py run <pairs> r2-20|r2-60 --duels 2`, then `analyse`),
only while no human is online. Pass marks: mage against rogue at level 60: the rogue reaches the mage in every duel and
casts a speed spell at most once per 3 s; warlock against paladin: share within 8 yd under 70 percent at level 20 (no
Pursuit of Justice) and a step back after every Mortal Coil; no `room=0` for more than 2 s in a row; hunters equipped with
a ranged weapon (`botscenario <hunter> equip` repairs a hunter at once): `select ... slot 15 subclass` shows no melee weapon.
For the new options grep `[1V1]` for `choice=defensive`, `choice=cc-break`, `choice=cover` and check the casts of the
watched defensives at the tier lines; the cover rule needs a lane with a wall (see above).

## Baseline tables (2026-11-18, live build 10, GM Island bench)

Two duels per pair. "within 8" = share of the duel within 8 yd; "moving" = share of samples the first bot moved; casts are
counted from the spell watch.

| Pair (tiers) | Winners | within 8 | mean yd | moving | First bot casts | Second bot casts |
|--------------|---------|----------|---------|--------|-----------------|------------------|
| hunter (expert) v warrior (casual), 20 | hunter, hunter | 97%, 95% | 3.6, 3.8 | 1%, 0% | 13 Disengage, none moved it | nothing watched |
| mage (expert) v rogue (good), 20 | rogue, rogue (mage fled) | 40%, 66% | 14.6, 9.4 | 5%, 15% | 2 Frost Nova, 3 Blink | 6 Crippling Poison |
| warlock (good) v paladin (good), 20 | paladin, paladin | 100%, 100% | 3.9, 3.9 | 0%, 0% | nothing watched | nothing watched |
| warrior (good) v rogue (expert), 20 | warrior, rogue | 100%, 91% | 3.9, 3.9 | 8%, 6% | nothing watched | Evasion |
| hunter (good) v mage (expert), 20 | hunter, hunter | 0%, 0% | 9.0, 9.0 | 0%, 0% | nothing watched | 6 Frostbolt |
| hunter (good) v warrior (good), 60 | hunter, hunter | 92%, 95% | 3.9, 3.7 | 5%, 0% | 6 Disengage, none moved it | 1 Charge |
| mage (good) v rogue (casual), 60 | rogue (mage fled), mage | 41%, 42% | 13.8, 9.4 | 8%, 0% | 2 Frostbolt, 3 Frost Nova or Ring of Frost, 2 Blink | 9 Burst of Speed or Sprint |
| warlock (good) v paladin (good), 60 | paladin (one request never answered) | 97% | 3.9 | 0% | nothing watched | 4 Rebuke |
| warrior (good) v rogue (casual), 60 | rogue, warrior | 94%, 89% | 4.0, 4.4 | 0%, 4% | nothing watched | 20 Crippling Poison hits, 2 Sprint |
| hunter (good) v mage (good), 60 | hunter, hunter | 0%, 21% | 9.0, 17.1 | 0%, 0% | Binding Shot, 1 Disengage | 3 Frostbolt, Ring of Frost, Frost Nova, Blink |

Natural duels at Goldshire and Razor Hill (six, levels 8 to 10): every duel with a melee side stayed within 8 yd for the
whole duel (a level 9 hunter against a shaman included; a priest stood 1.5 yd from a paladin); the casters against each
other stood 18 yd apart. No watched spell was cast.

## Build 14 bench (2026-11-18, before round 2)

Two duels per pair, same lane. Raw rows and traces of the bench runs b14-20 and b14-60 (CSV, SVG, event lists) are not published.
"within 8" = share of the duel the two stood within 8 yd; "B close" = seconds the melee side needed to come back within 8 yd
after each separation beyond 10 yd.

| Pair (level) | Winners | within 8 | mean yd | A moving | A casts (slow, root) | B casts | B close (s) |
|--------------|---------|----------|---------|----------|----------------------|---------|-------------|
| hunter v warrior (20) | hunter, hunter | 50%, 39% | 12.5, 9.6 | 18%, 11% | 2 Concussive, 1 Scatter, 1 Disengage | Charge 1 and 2 | 4.5 5.0; 3.0 2.5 7.5 |
| mage v rogue (20) | rogue, rogue | 47%, 50% | 10.1, 10.0 | 28%, 32% | Frost Nova 1, Blink 1 | 5 to 6 Crippling Poison | 8.0 1.5; 2.5 8.0 |
| warlock v paladin (20) | paladin, paladin | 97%, 97% | 3.9, 3.9 | 0%, 0% | nothing cast | nothing cast | |
| warrior v rogue (20) | rogue, rogue | 93%, 95% | 4.1, 4.0 | 0%, 0% | none | none | |
| hunter v mage (20) | hunter, hunter | 0%, 0% | 9.0, 9.0 | 0%, 0% | none | 4 to 5 Frostbolt | |
| hunter v warrior (60) | hunter, hunter | 68%, 75% | 6.3, 5.6 | 19%, 11% | 2 Concussive, Scatter, 1 to 2 Disengage | 7 to 8 Hamstring, Charge | 1.5 3.0 3.5; 0.5 2.0 |
| mage v rogue (60) | rogue, mage | 33%, 28% | 13.7, 13.2 | 27%, 28% | Cone of Cold or Frostbolt 3 to 9, Frost Nova 2 to 4, Blink 2 to 4 | Crippling Poison, Shadowstep or Sprint 3 to 6 | 5 to 11 s per separation |
| warlock v paladin (60) | paladin, paladin | 93%, 90% | 4.2, 4.2 | 5%, 8% | 8 Curse of Exhaustion, 3 to 4 Mortal Coil | 3 Hand of Freedom | 4.0; 2.0 3.0 |
| warrior v rogue (60) | rogue, warrior | 95%, 90% | 4.0, 4.4 | 1%, 7% | none | 8 Crippling Poison | |
| hunter v mage (60) | hunter, hunter | 0%, 0% | 9.0, 9.0 | 0%, 0% | none | 2 to 4 Frostbolt | |

The hunter versus mage pairs end in 13 to 18 s with both standing 9 yd apart: two casters trading spells, no kiting needed.

Natural duels at Goldshire and Razor Hill on build 14 (six, `logs/duel-bench/nat-b14`, 15 minutes of watching): every one
within 8 yd for 100 percent of the time, mean 4.0 yd, nobody moved, no watched spell cast. The duellists are level 6 to 12
(priest, monk, hunter, shaman, paladin): at those levels the kit does not exist yet (no Concussive Shot, Disengage, Psychic
Scream or Frost Nova), and a ranged bot at the same run speed gains nothing by running, so the engine holds and the rotation
fights; the log shows `choice=hold` in every decision. Two hunters read a level 8 shaman as a caster (`ranged-enemy`, correct:
Lightning Bolt). Nothing to fix there; the natural samples only become informative once the duel spot bots are level 15 or
more.
