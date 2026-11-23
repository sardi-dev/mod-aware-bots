# PvP awareness and the shared class rules R1 and R2

Status (2026-11-18): branch `feat/pvp-awareness-r1-r2`, unit tested and syntax checked, not built, no live measurement yet.
Source: design decision 0032 (point 5, tiered awareness; fake casting), the owner's answers of 2026-11-18, the fundamentals sheet
(topics 1, 2, 4, 9 and 14) and the class rules map (R1, R2, R5, R11).

## 1. Awareness: what a bot saw the enemy do (BT-520)

Every bot keeps a **book** of enemy state. The book holds observations, never the core's hidden state:

| Item | What is booked | Reading |
|------|----------------|---------|
| Trinket | the PvP medallion or Every Man for Himself seen used | DOWN for 85 percent of 120 s, MAYBE until 120 s, READY after |
| Cooldowns | big defensives, externals, cast stoppers, control breakers, stuns of the catalog | the same window per spell (catalog cooldown) |
| Kick | the stoppers of the enemy's class (a player sees the class on the frame) | UNKNOWN until one of the sure ones is seen used; DOWN when all the sure ones were |
| School lock | a kick or silence seen landing on a cast | the lockout of the spell, for the school it stopped (a silence: every school) |
| Diminishing returns | a crowd control seen to land on a target, with the duration of its aura | step 0 full, 1 half, 2 quarter, 3 immune; reset 18 s after the last effect ends |
| Last seen | where an enemy was when it last cast in sight | expires by tier (stealth) |

The cooldown window (85 to 100 percent is MAYBE) allows for talents and glyphs that shorten a spell. An item never seen is
UNKNOWN, not ready: `AssumeReady` is what the good and casual tiers use, `KnownReady` the strict reading.

### Tiers (proposals, tune with the benches)

| | Expert | Good | Casual |
|---|---|---|---|
| Sight | 70 yd | 50 yd | 35 yd |
| Trinket and racials | all enemies, 98 percent | healer and focus target, 90 percent | not tracked (assumes ready) |
| Big defensives | all, 98 | immunities of the healer and focus target, 85 | all it sees, 50 |
| Cast stoppers | all, 98 | the nearest kicker, 80, remembered 5 s | not tracked |
| Crowd control landed | all, 98 | its own and the key targets', 90 | its own, 60 |
| Memory | the whole match | 60 s | 15 s |
| Diminishing returns planning | exact | exact | disregards half of them (stable per target and category) |
| Last seen position | 8 s | 5 s | 2.5 s |
| Mistakes on purpose | 5 percent | 15 | 30 |

One roll per event and observer (the sequence number of the event), never a fresh roll per tick, so a bot that missed an
event misses it for good and the same moment replays the same way.

### Where the events come from

`PlayerScript::OnSpellCast` fires when a player's cast completes, before the cast is checked, so on its own it only proves an
attempt. The hook (`src/PvP/PvpAwareness.cpp`) queues an event for the catalog spells cast in a PvP context (battleground,
arena, duel, PvP flag, or aimed at another player) with the caster's position. The bot that reads the book later, on its own
map thread, waits 250 ms per event and then confirms what a player would have seen:

- a cooldown that started (the cast went off) for stoppers, defensives, externals and breakers;
- an aura of that spell by that caster on the target, with its remaining duration, for crowd control (this gives the
  duration the diminishing returns step needs);
- the medallion is booked as an attempt (its cooldown belongs to the item and is not readable).

Then the sight facts are taken from where the BOT stands (distance, line of sight, `CanSeeOrDetect`) and the tier decides.
Not covered: casts of pets and totems (no core hook; a Felhunter's Spell Lock is not booked), effects that need the aura of
another spell id than the cast.

Config: `AiPlayerbot.Pvp.Awareness.Enable` (1). Files: `Utils/PvpSpellCatalog.h` (spell data, ids checked against the 5.4.8
Spell.dbc names), `Utils/PvpAwarenessPolicy.h` (the book), `PvP/PvpAwareness.{h,cpp}` (hook and reading).

### Decisions this follows

- Team intel is shared only through visible /bg call-outs: books are per bot, never merged.
- Bots plan with the 5.4.8 diminishing returns categories (Freezing Trap, Ring of Frost, Chastise and Blinding Light are
  incapacitates and disorients with a full chain) and an 18 s reset; they do not depend on or exploit the core's gaps.

## 2. R1: what to kick, and when (BT-521)

The class interrupt actions and triggers are unchanged: a Rebuke, a Wind Shear, a Counterspell is still cast by the class's own
action. What changed is the question "is this cast worth the interrupt, and is it time". It is asked in four places, all behind
the one gate `strategy/generic/PvpInterrupt.{h,cpp}` (pure rules: `Utils/PvpInterruptPolicy.h`):

| Place | Question |
|-------|----------|
| `PlayerbotAI::IsInterruptableSpellCasting` (every `InterruptSpellTrigger`, the enemy healer value) | Wants: is this cast worth a kick at all (no clock) |
| `InterruptSpellTrigger::IsActive`, `InterruptEnemyHealerTrigger::IsActive` | Allow: the moment has come |
| `CastSpellOnEnemyCasterAction::isUseful` (every "<kick> on enemy caster" action), `CastPetInterruptAction` (Spell Lock, Optical Blast) | Allow |
| `pvp enemy caster target` (new value, the target of those actions) | the best cast by score, a cast the rules hold is passed over |

The plain `enemy caster target` value and the "enemy caster in range" trigger keep their meaning (the death knight's
Anti-Magic Shell cue uses them); only the interrupt actions aim at the PvP value.

A monster, or any caster that is not a player, a player's pet or a player's totem, is not governed: the older paths and the
dungeon interrupt turns stay as they were. `AiPlayerbot.Pvp.Interrupts.Enable = 0` restores the old behaviour everywhere.

### What is worth a kick (score, highest first)

| Cast | Score |
|------|-------|
| Crowd control on our healer | 1000 |
| Crowd control on the bot | 950 |
| Crowd control on another friend | 900 |
| A heal the enemy healer casts on itself while it is our kill target | 850 |
| A heal on a target at or under 40 percent (our kill window) | 800 |
| The same up to 60 percent while our own burst is running (not detected yet, see below) | 750 |
| A large damage cast (2 s cast or any channel) on a friend at or under 70 percent | 600 |
| A kick on filler (a mistake, or a casual bot) | 100 |
| Everything else | held |

A control cast is a spell of the control categories of the catalog (stun, incapacitate, disorient, fear, horror, mind control,
cyclone) or, for a spell the catalog does not list, any of those mechanics. A cast of an enemy healer in a school it does not
heal with (shaman, druid, monk Nature; priest, paladin Holy) is held: the lock would close the wrong school. A school that a
partner's kick already closed on that caster (read from the bot's own book) is not kicked again.

### When

The first look at a cast is time zero. A bot of the expert tier lets the kick go at 60 to 90 percent of the cast time (one
value per cast, so a player cannot learn the pattern), a good bot at 70 percent, a casual bot as soon as it noticed. Never
before the tier's reaction time (250 to 350, 350 to 500, 600 to 900 ms as in the duel engine), never later than 300 ms before
the cast ends; a cast that ends before a player could react is kicked at once; a channel is kicked after the reaction time. A
second interrupter of the group (the order of `MopDungeon::InterrupterRank`) waits 500 ms per rank behind the first: it kicks
only if the cast still runs.

### Tiers

| | Expert | Good | Casual |
|---|---|---|---|
| Sees | every casting enemy | its target, the enemy healer, casts on itself | its own target |
| Filler cast | holds, kicks it 5 percent | holds, kicks it 30 percent | kicks it |
| Turns | all | all | misses one in three |

### Not done

- Our own burst (`ourBurst`) is read as false: the heal window is 40 percent only. It needs the team go state (R6).
- Interrupts that need a spec (Silence, Strangulate, Avenger's Shield, Solar Beam) go through the same triggers; only the
  ones the class strategies already cast are gated.
- No interrupt rotation by fixed roles (primary on the healer, backup on the rest): the order is the group's existing one
  (tanks, melee, ranged, then by GUID). In a battleground the rank counts only bots of the same raid group within 45 yd.
- Debug line per cast: `[PVPINT] <bot> caster= spell= class= why= score= at= rank= tier= school= onus= side= hp=`.

## 3. R2: defensives and ways out of control (BT-522)

One place reads what a player sees and asks the pure rules (`Utils/PvpDefensivePolicy.h`, glue
`strategy/generic/PvpDefense.{h,cpp}`); the 1v1 engine, the arena plan and a new strategy `pvp defense` use the answers.

| Where | What changed |
|-------|--------------|
| 1v1 engine (duels, lone fights, isolated battleground skirmishes) | `defensiveReady` and `ccBreakReady` are the rules' verdict; the health line of the engine stays only with `AiPlayerbot.Pvp.Defensives.Enable = 0` |
| Arena plan (ladder teams) | the plan still says WHEN (its score and predicted lethal burst); WHICH spell is the rules' pick (tempo first, immunity last) |
| Battleground and arena groups (all bots, new strategy `pvp defense`, triggers `pvp defensive due` and `pvp break due`) | a defensive at the rules' moment, and the way out of control; silent for the ladder teams' defensive (the plan has it) and in a 1v1 (the engine has it) |
| Racial triggers (Every Man for Himself, Will of the Forsaken, Escape Artist) | stand down in a battleground, an arena and a 1v1: the break is the engine's or the strategy's action, so the cheapest break wins |

### Defensives

- A **go** is on when somebody hits the bot and it lost 20 percent of its health in 3 s, or two enemies hit it.
- Tempo defensive at or under the tier's line under a go (expert 80, good 65, casual 35), without a go at 50, 45, 35.
- **Immunity last**: only at 30 percent (casual 25), or when death is predicted (80 percent of the current health lost in 3 s);
  then the immunity goes first, the one-per-go rule is lifted.
- **One per go**: nothing while a defensive runs or within 4 s of the last.
- The shortest cooldown first inside the tempo defensives (Barkskin before Survival Instincts).
- Fit: Evasion and Die by the Sword answer melee, Cloak of Shadows and Anti-Magic Shell casters; a hunter's arrows need neither.
- Silence-blocked (Divine Protection): an expert that expects a silence (an attacker whose class has one, not seen used)
  presses it 10 points above its line.
- Mistakes on purpose: a good or casual bot sometimes presses late (the line sinks 15 points for that go; one roll per go).
- The candidates are the catalog rows of the class that the bot knows, that are off cooldown and not running, and that the
  core accepts (`CanCastSpell`), so a defensive that cannot be cast while stunned or silenced is never chosen.

### Breaks from control

The cheapest ready candidate that removes the worst effect (the hard kind with the longest time left):

| Order | Candidates |
|-------|------------|
| 0 class | Berserker Rage (fear, sap, incapacitate), Lichborne (fear, charm, sleep) |
| 1 own cooldown | Will of the Forsaken (fear, charm, sleep), Unbound Will (everything the medallion removes), Escape Artist (root, snare) |
| 2 shared | Every Man for Himself (the medallion's two minutes) |
| 3 | the PvP medallion |

A cheap break is used whenever at least 1.2 s of the effect is left. Cost 2 and 3 (the medallion family) only when the role wants it:

- **Healer**: during the enemy kill window (a teammate at or under 40 percent, or the bot or a teammate in a go) with at
  least 3 s of the effect left; otherwise it eats the control.
- **Damage dealer**: when the enemy healer is held for 2 s more and its kill target is at or under 35 percent, or when it
  would be at or under 35 percent when the effect ends (recent loss rate times the time left).
- At 20 percent projected, always.
- **Expert** holds it when the awareness book says the enemy has a control of a fresh category that is ready (not DR immune on the bot).
- A movement impairment alone is broken only when the bot has somewhere to be (a ranged bot in a 1v1, a chaser, a carrier).
- Mistake on purpose: the medallion at full health on an effect that did not need it (expert 5 percent, good 15, casual 30).

### Not done

- The class strategies' own health-line defensives (Shield Wall at 40 percent and the like) still fire next to these rules, and
  do not know the one-per-go count; folding them in is class work and not part of R2's shared layer.
- The kill window is read from health and the bot's own loss rate; a team go state (R6) will replace the guess.
- "Press before the silence" needs a silence that was seen coming or a silencer class; it is conservative.
- Arena healers' trinket by `enemyBurst` uses the bot's own loss and teammates' health, not the enemy's cooldowns.

## 4. R11: the fake cast (BT-523)

How casts are issued and cancelled (found while reading the paths): the class actions call `PlayerbotAI::CastSpell`, which builds the
cast and calls `Spell::prepare`; while a cast with a cast time runs it sits in the unit's `CURRENT_GENERIC_SPELL` slot in state
`SPELL_STATE_PREPARING`, and the AI keeps ticking (the 1v1 engine already cancels a cast by walking). `Unit::InterruptSpell(
CURRENT_GENERIC_SPELL)` runs `Spell::cancel`: it gives back the global cooldown, takes no mana and starts no cooldown (those come
when a cast completes), and tells every player in range that the cast failed (SMSG_SPELL_FAILURE and SMSG_SPELL_FAILED_OTHER).
So a cancelled cast costs the time it ran and nothing else; the rotation casts again on its next tick. The module's
`PlayerbotAI::InterruptSpell()` is not used: it also cancels the auto shot and melee swings.

`strategy/generic/FakeCast.{h,cpp}` (pure rules in `Utils/FakeCastPolicy.h`, strategy `pvp cast` in every combat engine, trigger
`fake cast due`, action `fake cast`):

1. **First look** at the bot's own cast (once per cast): important? (crowd control; a heal on a target at or under 70 percent; a
   damage cast of 2 s or more; never a channel or a cast under 1.5 s) and not an emergency (a heal on a target at or under 25
   percent, the bot at or under 20 percent).
2. A **threat**: an enemy player in sight (`CanSeeOrDetect`), within 45 yd, in line of sight of the bot, facing it (180 degree
   arc), not stunned, feared or confused, whose class has a sure cast stopper in range (the range of the spell in the 5.4.8 data
   plus a yard), and the bot's book did not see that stopper used within its cooldown (unknown counts as ready).
3. **One roll per cast**: `ChancePercent` (65) of the important casts under threat are fakes. The cancel point is 45 to 85 percent of
   the cast time, fixed per cast, never later than 250 ms before the end.
4. At the cancel point the threat and the emergency test are made again; then the cast is cancelled. The next cast within 8 s is real.
5. Only the expert tier does it, with the owner's decision (60 to 70 percent of the important casts while an enemy kick is ready
   and in range): the measured share is in the log line.

Log: `[FAKECAST] <bot> spell= decision=fake|real why= cancel_at= cast_ms= kicker= heal_hp=` per important cast, and `... cancelled`.
The share of fakes among the lines with `kicker=1` and `why=fake` or `why=roll-real` should be 60 to 70 percent.

### Not done

- A fake is not tied to the enemy's reaction: a kicker that bites on the fake shows as a kick in the book (the interrupt is booked
  with the bot as the target) and the next cast is cast for real because the stopper then reads DOWN.
- Pets' stoppers (Felhunter) and spec stoppers (Silence, Strangulate) are not counted as threats (not booked until seen).
- Instants are never faked; casters that move (Spiritwalker's Grace, Ice Floes) are not special-cased.
- Whether a human sees the fake as a fake (the cast bar disappears and the failure message shows) needs the owner's in-game test.

## What the duel bench should measure afterwards

`tools/bots-bench/duel/` on the fixed pairs (levels 20 and 60):

- `[1V1]` lines: `choice=defensive` at 70 to 80 percent of health under a go for an expert (not only at 55), `choice=cc-break` with
  Berserker Rage, Lichborne or Will of the Forsaken chosen before Every Man for Himself or the medallion; no medallion at full
  health unless a mistake (expert 5 percent); a paladin's Divine Protection before Divine Shield.
- `[PVPINT]` lines in duels with a caster: a Fireball or Frostbolt at a healthy bot is held (`why=hold-filler`), a Polymorph on the
  bot is kicked at 60 to 90 percent of its cast (`at=` against the cast time), a priest's heal at or under 40 percent is kicked.
- `[FAKECAST]` lines: the share of fakes among the casts with `kicker=1` (60 to 70 percent), `cancelled` lines about 45 to 85
  percent into the cast, no fake in an emergency, the cast after a fake real.
- Win rates per tier pair (expert against good and casual) before and after the three switches
  (`AiPlayerbot.Pvp.Interrupts.Enable`, `.Defensives.Enable`, `.FakeCast.Enable`) to see what each rule is worth; a mage against a
  rogue and a priest or shaman healer against a melee class are the pairs where the three rules show.
- The awareness book: a kick or a medallion seen on a bot should read DOWN for 85 percent of its cooldown (debug log of the book is
  not in this branch; read it through the fake cast's `kicker=` field and the interrupt lines).

## Owner tests needed (class and spell behaviour)

The branch changes what bots do with spells; nothing goes upstream before the owner's in-game test:

1. A duel against an expert mage or priest: do the kicks land late and on the right casts, does the Polymorph get kicked and the
   Fireball not?
2. A duel against an expert warrior or paladin: the first defensive early (Divine Protection, Shield Wall) and the immunity last.
3. A stunned or feared bot: Berserker Rage, Lichborne, Will of the Forsaken before the medallion; a stun at full health is eaten.
4. A caster's fake cast: the cast bar of an expert mage or priest disappears at about half the bar when your kicker is ready and
   in range; the next cast goes through.
