# Role sheet: tank (all five tank specialisations)

- **Status:** draft of 2026-11-12, first version, written from reading the code at `main` (commit b518acb) and the
  Deadmines run of the same day. Nothing here was run. A line is **read** when a call site was read, **inference** when it
  follows from the numbers but no log shows it, **to check live** when only a test can tell. Levels, ids and costs come
  from the class sheets (`docs/class-sheets/`, 5.4.8 data); this sheet does not repeat the rotations.
- **Specialisations:** protection warrior, protection paladin, guardian druid, blood death knight, brewmaster monk.
  Class sheets: `warrior-protection.md`, `paladin-protection.md`, `druid-guardian.md`, `deathknight-blood.md`,
  `monk-brewmaster.md`. This sheet owns what the five share in a group: owning the fight, keeping every mob off the
  others, reading the group.
- **Owner direction (2026-11-10 and 2026-11-12):** the bot tank pulls marks only (skull) or what attacks the group; it
  waits for the leader, never pulls while a member is far, dead, low on health or low on mana; healers and casters drink
  below 50 percent between pulls; a mob on any member is taken at once (BT-155). Pains named by the owner: tank and pulls,
  stacking on the leader, bad paths, robotic chat and timing, no initiative.
- **Skill tiers (design decision 0017, 10 percent expert, 60 good, 30 casual):** every rule is marked **core** (all tiers), **good**
  (good and expert) or **expert**. PvE stays deterministic (design decision 0014): none of these rules rolls dice or asks a model.
- **Scope:** five-player dungeons first (a human leader who marks, three to four bots); raids in section 9.
- **Size key:** S under about 50 lines plus a test, M up to about 300 lines or a new value, L a new system or navigation work.

## 1. Evidence used

| Source | What it shows |
|--------|---------------|
| the defend diagnosis notes (Deadmines run 1) | Four unmarked mobs killed the leader in 39 s; the protection paladin tank stood out of combat at 100 percent until 0.9 s after the death, in the non-combat engine. The three causes read there (no non-combat taunt, `lose aggro` only on the current target, BT-118 limited to hidden attackers) are fixed by BT-155 (`77fb012`, on `main`). Why the tank did not engage was never proven (candidates: line of sight, `IsPossibleTarget`, a stale target). |
| `logs/phase2/dungeon/deadmines.world.log` | 8 `[PULL]` lines for the tank Ernac in 20 minutes, all `accepted`, 13.9 to 25.0 yd, line of sight 1 (Avenger's Shield 7 times, Reckoning once). The line is limited to one per target per 20 s, so these are 8 distinct pulls. |
| `logs/phase2/dungeon/deadmines.sync.log` and `.report.txt` | 33 pack marks: 22 ended with the marked creature dead, 4 ended "no fight started within 45 s of the mark" (pulls 13, 15, 16, 22; one is a Mine Bunny, a harness artefact), 11 had nothing to mark. Median mark to kill 17 s, longest 121 s. For three of the four stalled marks the log has no `[PULL]` line at the time. The bot tank therefore opened roughly 8 of 22 killed marks; who opened the rest is **not shown** (candidates: the leader walking in, the 45 s fallback of `HoldMarkForTank`, a blocked readiness test). |
| `deadmines.client.log` | The tank sent "All paths not in LOS" 39 times in 23 minutes (the healer 51, a damage dealer 29). The text is `MovementAction::MoveNear` (`MovementActions.cpp:687`) giving up. Callers that can run on a tank: the flee moves (`:1346`, `:1350`, `:1421`), `LootAction.cpp:589`, `SeeSpellAction.cpp:522`. Which one fired is **not shown**. |
| Findings rows | BT-112 (zero threat division), BT-113 and BT-141 (marks held for the tank), BT-118 and BT-155 (rescue), BT-119 (left behind), BT-139 (guardian self heal), BT-143 (pull line of sight). Note: the findings ledger (kept outside this repository) on `main` has no BT-155 row; the commit message is the only record. |

## 2. What a good tank does

### Before the pull
- **Waits for the group** (core). A mark is pulled only when every member is alive, out of combat, within the mark pull
  range of the tank (40 yd), above 60 percent health, and (mana users) above 50 percent mana. Today:
  `triggers/MarkTriggers.cpp` `MarkedTargetReadyTrigger::IsActive`, `MemberReady`; strategy `MarkPullStrategy.cpp` (relevance 50);
  config `AiPlayerbot.TankPullsMarks`, `AiPlayerbot.MarkPullRange`. Done.
- **The tank is ready itself** (core): health at 80 percent or more (the first hits land on it, and 60 is the number the
  rule uses for squishier members), stance, form, presence or aura up (class sheets, all five have a non-combat line now:
  `GenericWarriorNonCombatStrategy.cpp` "tank defensive stance", `GenericDKNonCombatStrategy.cpp` "tank blood presence",
  `GenericDruidNonCombatStrategy.cpp` "tank bear form", `MonkStrategies.cpp` "stance of the sturdy ox", paladin seal and
  Righteous Fury). Health at 80 is **not done** (gap 4).
- **The healer is able** (core): alive, in range, mana at or above the drink threshold; (**good**) in line of sight of the
  tank, and mana at 70 percent or more before a boss mark. Range and mana are done, sight and the boss rule are not.
- **The leader is not in the way** (good): the leader is not closer to the mark than the aggro radius (about 10 yd for
  trash). Not done; the Deadmines death came from the leader walking onto an unmarked pack, which no tank rule can undo.
- **Says why it waits** (good): after about 10 s of holding a ready mark, one party line naming the reason ("waiting for
  Emidan's mana", "waiting for Walkdraenei"), once per mark. Not done (gap 3). The healer already asks to drink
  (`DungeonChat`); the tank has only the 12 percent "Pulling, get ready" roll (`PullActions.cpp` `BotVoice::BEFORE_PULL`).
- **Marks only** (core). The pull target is the skull (`RtiValue` default "skull"; `MarkedTargetReadyTrigger` falls back to
  the group's own icon when the "rti target" value cannot see the mark, because it wants sight). Other icons are not pulled.
  The moon (index 4) is the crowd control icon and the tank never attacks it (`TankTargetValue.cpp`
  `FindTankTargetSmartStrategy::CheckAttacker`). Done.
- **Damage dealers wait too** (core): an untouched skull is kept out of the attackers of a five-player group with a bot tank
  that has `mark pull`, for at most 45 s per bot (`AttackersValue.cpp` `HoldMarkForTank`, `HELD_MARK_FALLBACK_SECONDS`;
  `DpsTargetValue.cpp` `IsTanksPull`). It yields at once when a member is attacked (`Utils/DefendPolicy.h`). Done (BT-141,
  BT-113, BT-155). A real player tank and raids are not held.

### The pull
- **Ranged pull with the class tool** (core), line of sight included (BT-143): `PullActions.cpp` `PullAction::Execute`,
  `IsWithinPullRange` (range and `IsWithinLOSInMap`), `ReachPullAction` (walks to range, at worst to melee when the mark is
  unseen). Spells in section 3. Done.
- **Pull back** (core): after the hit the tank returns to where it stood when it asked for the pull (`PositionMap` "pull",
  `ReturnToPullPositionAction`, `PullEndTrigger`). That spot is wherever the tank stood, usually beside the group, so the
  mobs run to the group instead of the tank running to them. Done, but it is not chosen for sight (below).
- **Line of sight pull** (expert, gap 14): choose the pull-back spot behind a corner or pillar so casters and ranged mobs must
  come into melee. No navigation query exists for this; it is the one L item.
- **Gather the pack** (good): after the pull, the area threat move of the class once the pack arrives (section 3 and 4). The
  edges exist per class ("light aoe" two within 8 yd of the target, "medium aoe" three, "high aoe" four; `GenericTriggers.h`
  `AoeTrigger`). Done for the casters that arrive in melee; a caster that stands back is a loose mob (next part).
- **Never pull through another pack** (good, **inference**: not read, no path check is in the pull code): refuse a pull whose
  straight line to the mark passes within the aggro radius of another living hostile that is not part of the mark's pack.
  Not done; needs a `possible targets` scan at pull time only (once per attempt, not per tick).

### Holding threat on many mobs
- **Target order** (core): `value/TankTargetValue.cpp` `FindTankTargetSmartStrategy::IsBetter`: a mob that does not have the
  tank as its victim first (nearest of them), then mobs the tank holds that are in melee, then by lowest tank threat. This is
  the loose mob pick-up. `TankAssistTrigger` (`GenericTriggers.cpp:542`) switches only while the current target already has
  aggro, so it does not abandon a mob it has not secured. Done.
- **Taunt** (core): `lose aggro` fires the class taunt when the current target's victim is not the tank
  (`GenericTriggers.cpp` `LoseAggroTrigger`; never on players). Because the pick-up above makes the loose mob the current
  target first, the taunt lands on it on the next tick. Edges: warrior `taunt` ACTION_INTERRUPT + 1, paladin `hand of
  reckoning` HIGH + 7, bear `growl` HIGH + 8, blood `dark command` HIGH + 3, brewmaster `provoke` INTERRUPT + 1. Done.
- **Defend a member** (core, BT-155): `generic/AttackedMemberRescue.cpp` `FindAttacker` picks the nearest hostile creature
  whose victim is a non-tank group member (no line of sight or health test); `ReachAttackedMemberAction` attacks it and
  casts the class taunt (`TauntFor`), from the non-combat engine too (`TankAssistStrategy.cpp`, ACTION_RAID). It does not run
  while a dungeon boss or world boss holds the tank (`TankHoldsBoss`), gives up after 8 s on an unreachable attacker and rests
  10 s (`RESCUE_MAX_SECONDS`, `RESCUE_COOLDOWN_SECONDS`). A rate limited `[DEFEND]` line in `Engine::DoNextAction` says why a
  bot held back. Done.
- **A mob on another floor** (good, BT-377, branch fix/floor-attackers-and-ledges, compile and live test pending): a mob that
  hits a member from a ledge or floor over (or under) the group cannot be reached, and the 8 s walk, the give-up and the
  10 or 60 s rest repeated 7 times in 330 s in the Stockade (Hogger, 5 yd up) and 165 s in Shadowfang Keep (a Frantic Geist, 9.6
  to 16 yd down). A player tank steps away from the overlap so the mob must come to him, then taunts. Now:
  `ReachAttackedMemberAction::Execute` at a give-up with `FloorAttackerPolicy::NeedsStepAway` (3 yd or more of height, within 6
  yd flat of the victim, within 40 yd of the tank) calls `AttackedMemberRescue::BeginStepAway`: 36 spots on rings of 12, 14 and 16
  yd round the tank, on its floor, at least 6 yd farther from the mob, not within 20 yd of another awake hostile that is not
  fighting (the cached "possible targets" list), spots that see the mob first, then the nearest, four path searches at most, a
  whole and short path; the tank walks there and holds until the mob is in sight, another attacker is in sight or 15 s pass (`FindAttacker` skips that mob meanwhile unless it is in sight)
  (`StepAwayFromFloorAttackerAction`, trigger `floor attacker step away`, relevance 45 in `TankAssistStrategy.cpp`). The member
  under attack steps away the same way once the tank gave up on that mob (`FindFloorAttackerOnMe`, relevance 25 in
  `DpsAssistStrategy.cpp`, below the critical heal). Sight returns the mob to the normal rescue and its taunt
  (`TankRescuePolicy::Consider`: sight beats a give-up). Limits: 3 steps per mob in 3 minutes, one search per 20 s, a retry
  keeps 5 yd from the last spot. It never acts with a human tank, in raids it still applies (dungeon maps, `IsDungeon`).
  Not done: the step does not stop the give-up's walk spline (the Shadowfang Keep tank rose 8 yd after a give-up, see
  BT-388), and a mob with no way down never comes; after three tries the old give-up rules stand.
  What it does not do: take a mob **from range** with a gap closer or pull (gap 5); choose the attacker that matters most
  (the one on the healer, or on the weakest member) instead of the nearest (gap 5); use an **area** move when two or more
  mobs hang on members (gap 6); abort a pull in progress (gap 1).
- **Caster and healer mobs first** (good, gap 7): among loose mobs, a caster or healer before a melee mob. Today the order is
  by distance only (`IsBetter`, interval level 2).
- **Off-tank** (good): with two tanks the main tank keeps its current target (`GetGroupTankNum` branch in `IsBetter`). Done.

### Positioning
- **Faces the mob away from the group** (core): `actions/MovementActions.cpp` `TankFaceAction::Execute`
  (strategy `tank face`, added to every tank, `AiFactory.cpp:553`): when the tank holds the target in melee and the target is
  not moving, it steps to an angle 3/5 pi off the group's average direction. Done. It is a no-op without a group, with a
  moving mob or without aggro, so it does not drag a mob that is still being pulled.
- **Does not move the mob needlessly** (core): the same action stops when the angle is already tolerable (within pi/2). Boss
  modules zero it where position matters (for example `dungeons/CataDm/CataDmMultipliers.cpp`). Done.
- **Out of bad effects** (good): `AvoidAoeAction::AvoidUnitWithDamageAura` (`MovementActions.cpp:2034`) moves off trigger
  creatures that carry a periodic trigger aura, only for bots with a real player master and `AutoAvoidAoe` on. The
  dynamic-object and damaging-trap cases are not ported. So a fire patch from a spell is not avoided, by the tank or anyone
  (gap 12; shared with every role).
- **Does not stack on the leader** (core, owner pain): out of combat the tank follows like everyone, through `Formations`
  (a tank in front when the leader is not a tank: `value/Formations.cpp:480`). Following belongs to the follow work, not to
  this sheet; the tank-specific part is that it stays within the mark pull range so the readiness test can pass.

### Cooldowns by danger
- **Today:** every class defensive reads the tank's **own health** only: warrior Shield Wall 40, Last Stand 35, Enraged
  Regeneration 50 (`TankWarriorStrategy.cpp`, `WarriorTriggers.h`); paladin Divine Protection 65, Ardent Defender 40, Lay on
  Hands 20 (`PaladinActions.h` `TANK_*_HEALTH_PCT`); blood Icebound Fortitude 50, Rune Tap 60, Vampiric Blood 60
  (`DKTriggers.h`); bear Frenzied Regeneration at medium health, Savage Defense at 60 rage; brewmaster Fortifying Brew at
  low health, Dampen Harm 45, Guard on cooldown. Good thresholds, but all late for a burst: a tank at 100 percent can die
  to one boss cast chain.
- **Rule** (good, gap 8): one shared danger reading per tank, 0 to 3, from (a) the number of mobs attacking the tank, (b)
  the share of maximum health lost in the last 5 s, (c) no living healer in range, (d) a dungeon boss or a casting elite
  targeting the tank. Level 1 allows the first defensive 10 percentage points earlier, level 2 allows the big one at 60
  percent, level 3 presses everything. It adds to the existing thresholds; nothing is removed. The pack-start cooldowns
  (Demoralizing Shout, Berserk, Avenging Wrath, Dancing Rune Weapon) fire on the first pull of three or more mobs.
- **Healer cooperation** (expert): Hand of Sacrifice (paladin 80) on the healer, Intervene (warrior 72) and Vigilance
  (talent 75) on the member that draws attacks; the code for the last two exists (`protect party member`, `vigilance`).

### Interrupts and stuns
- **Interrupts** (good): warrior Pummel on pack casters (`pummel on pack caster`), blood Mind Freeze on pack casters, paladin
  Rebuke on the target and on any casting enemy (`GenericPaladinStrategy.cpp`), brewmaster Spear Hand Strike on any caster
  (`MonkStrategies.cpp:189`). Bear Skull Bash is **PvP only** (`BearTankDruidStrategy.cpp` "skull bash on enemy caster";
  class sheet druid gap 6). Avenger's Shield also silences (it is the pull and the 15 s default).
- **No coordination** (inference, read of `PlayerbotAI::IsInterruptableSpellCasting`, `PlayerbotAI.cpp:3667`): the check is
  whether the bot's own spell has interrupt properties and the target is casting; it does not ask whether another bot has
  already kicked this cast or whether the cast is worth a kick. In a group with a tank, a rogue and a mage, two or three kick
  one cast and the next cast is free (gap 9). Not seen in a log.
- **Stuns** (good, gap 10): used today as damage filler or to interrupt: warrior Storm Bolt (default list, +0.4, talent),
  paladin Hammer of Justice (7, interrupt edges), bear Bash. Rule: a stun on a caster whose interrupt is on cooldown, and on
  a mob that reaches the healer when the tank cannot take it back at once. None of the five has a "peel the healer" line.
- **Disrupting Shout, Strangulate, Leg Sweep, Paralysis, Clash** (expert): see class sheets; mostly behind talents or
  PvP-only conditions.

### Between pulls
- **Waits** (core) for the group, as above. **Eats and drinks** is the bots' general rule; the tank has no mana (except
  paladin and monk talent cases) and its health is covered by the 80 percent rule once it exists.
- **Holds the skull** (core): does not touch unmarked mobs, does not walk ahead of the leader. The pace is the leader's.
- **Marks for the group in a fight** (core): with a real player master only the main tank marks, and only a mob that
  attacks, never an icon that still sits on a living unit (`actions/RtiAction.cpp` `MarkRtiAction::Execute`, strategy `mark
  rti`, added to a tank at `AiFactory.cpp:567`). It marks the lowest-health attacker (kills the weakest first), not the
  caster first (gap 7).
- **Breaks and group chat** (good): the shared `break` and `dungeon chat` strategies; the tank has no tank-specific line.

## 3. Spells that fill each job (level, from the class sheets)

| Job | Prot warrior | Prot paladin | Guardian druid | Blood DK | Brewmaster monk |
|-----|--------------|--------------|----------------|----------|-----------------|
| Ranged pull | Heroic Throw (22), Taunt (12), Charge (3) fallback | Avenger's Shield (10, also silences), Reckoning (15), Judgment (5) | Faerie Fire (28), Growl (8) | Death Grip (55, taunts 3 s) or Icy Touch (55) by the pack rule | Provoke (14), Crackling Jade Lightning (54) while Provoke is down |
| Single taunt | Taunt (12, 30 yd, 8 s) | Reckoning (15, 30 yd, 8 s) | Growl (8, 30 yd, 8 s, bear form) | Dark Command (58, 30 yd, 8 s); Death Grip (55) before | Provoke (14, 40 yd, 8 s) |
| Area threat | Thunder Clap (20), Demoralizing Shout (56), Shockwave or Dragon Roar (60, talent), Mocking Banner (87, area taunt) | Consecration (34), Hammer of the Righteous (20), Avenger's Shield jump; **no area taunt** | Thrash (28), Swipe (22); **no area taunt** | Blood Boil (56), Death and Decay (60), Pestilence (56) | Keg Smash (11), Breath of Fire (18), Spinning Crane Kick (46), Dizzying Haze (10); Black Ox Statue area Provoke (70) |
| Gap closer and grab | Charge (3), Intervene (72), Heroic Leap (85) | none (Avenger's Shield at 30 yd) | Wild Charge (talent) | Death Grip (55) | Roll, Clash (18, stun) |
| Interrupt | Pummel (24), Disrupting Shout (talent 45) | Rebuke (36), Avenger's Shield | Skull Bash (64) | Mind Freeze (57), Strangulate (58) | Spear Hand Strike (32) |
| Stun | Storm Bolt (talent), Shockwave (talent 60) | Hammer of Justice (7), Holy Wrath (20, demons and undead) | Bash (talent: Mighty Bash) | Asphyxiate and Gorefiend's Grasp (talents) | Clash (18), Leg Sweep (talent 60), Paralysis (44, crowd control) |
| Small defensive | Shield Block (18), Shield Barrier (81), Spell Reflection (66) | Shield of the Righteous (40), Divine Protection (26), Eternal Flame | Savage Defense (10), Barkskin (44) | Rune Tap (64), Death Strike (56), Bone Shield (78) | Guard (26), Expel Harm (26), Elusive Brew (36), Dampen Harm (talent) |
| Big defensive | Shield Wall (48), Last Stand (38), Enraged Regeneration (talent 30) | Ardent Defender (70), Guardian of Ancient Kings (75), Lay on Hands (16), Hand of Protection | Survival Instincts (56), Might of Ursoc (72), Frenzied Regeneration (68) | Icebound Fortitude (62), Vampiric Blood (76), Anti-Magic Shell (68), Dancing Rune Weapon (74) | Fortifying Brew (24), Zen Meditation (82), Avert Harm (48) |
| Help for a member | Intervene (72), Vigilance (talent 75), Rallying Cry (83) | Hand of Sacrifice (80) | Stampeding Roar (84), Innervate (54), Rebirth (56) | Raise Ally (72) | Black Ox Statue Guard (70) |

Notes:
- No area taunt exists in 5.4.8 for the paladin and the druid (class sheets). The warrior has one at 87 (Mocking Banner), the
  brewmaster one at 70 through the statue. Everyone else holds many mobs with area damage on threat, so "area threat first"
  means the area attack, not a taunt.
- The warrior declares Taunt a melee action although it reaches 30 yd (`WarriorActions.h`, class sheet gap 8). BT-155's
  rescue casts it by name from line of sight, so it works at range there.
- The guardian needs Bear Form for Growl; the non-combat "tank bear form" now keeps the form up.

## 4. What the bots do today, in one table

| Job | State | Where |
|-----|-------|-------|
| Pull only marks, after the group is ready | done | `MarkTriggers.cpp`, `MarkPullStrategy.cpp`, `AiFactory.cpp:840` and `:903` |
| Others wait for the pull, with a 45 s fallback | done | `AttackersValue.cpp` `HoldMarkForTank`, `DpsTargetValue.cpp` `IsTanksPull` |
| Pull with line of sight, walk to range | done (BT-143) | `PullActions.cpp` |
| Walk back to the pull spot | done, not sight-aware | `PullTriggers.cpp`, `PullActions.cpp` `ReturnToPullPositionAction` |
| Defend any member, from the non-combat engine, with a taunt | done (BT-155) | `AttackedMemberRescue.cpp` |
| Pick up loose mobs by target order and taunt | done | `TankTargetValue.cpp`, `GenericTriggers.cpp` |
| Face the mob away from the group | done | `MovementActions.cpp` `TankFaceAction` |
| Defensives by own health | done (class sheets) | per class strategy |
| Pack interrupts | done except the bear | per class strategy |
| Pull blocks the defend, taunt and defensive actions | **bug (read)** | `PullStrategy.cpp` `PullMultiplier::GetValue` |
| Say why it waits, log why a mark was not pulled | not done | none |

## 5. Gap list (ordered by value, with size and tier)

State is at `main`. "Read" means a call site was read; "inference" means it follows from the code and no log shows it.

**Update 2026-11-16 (branch feat/tank-healer-polish, compile and live test pending):** gap 8 done as BT-292
(`generic/TankDanger.cpp`, `Utils/TankDangerPolicy.h`: one danger level 0 to 3 drives the small and big defensive of every
tank class, good and expert tiers; the pack-start cooldowns are still open); gap 12 partly done as BT-294 (the
dynamic-object case of `AvoidAoeAction`; traps still open); the healer-sight hold of Shadowfang Keep run 8 is BT-290 (the
tank steps into the healer's sight after her own walk, and sight stops holding a mark after 60 s). Gaps 1 to 7 and 9 were
done earlier (BT-160, BT-162, BT-164).

**Update 2026-11-17 (branch fix/proof-runs-night3, compile and live test pending):** the line of sight pull (the
"line of sight pulls" job of section 2) is no longer open for the case the live runs found: a mark the tank cannot see
and cannot walk into sight of. BT-333 (`ReachPullAction::WalkToSightSpot`, `Utils/PullSightPolicy.h`) tries spots on
rings of 8, 14 and 20 yd round the mark and walks to the nearest one that sees it with a short path; BT-332 sends a tank
that stays blind for 15 s to the leader's side, as marks beyond the group range already did (BT-200). The corner pull
(choosing a spot behind a corner so casters come to the group) and gathering casters stay open. Proof run findings: the
Deadmines ran clean from the entrance through Foe Reaper 5000 (three bosses, four watchers, no death) in the runs of 2026-11-16 and 17; open and unexplained is the healer that
ends on a navmesh island above the Shadowfang Keep courtyard (BT-331 logs it).

**Update 2026-11-18 (branch fix/floor-attackers-and-ledges, compile and live test pending):** gap 20 (a mob on another floor)
done as BT-377 and a stuck bot in a player's view as BT-388 (shared with every role, listed as gap 21): see section 2 "A mob
on another floor" and the rows below.

| # | Gap | Size | Tier | Evidence |
|---|-----|------|------|----------|
| 1 | **A pull blocks every action not on its short list, up to 15 s** (`PullMultiplier::GetValue`, `PullStrategy.cpp:186-203`, `GetMaxPullTime` 15 in `PullStrategy.h:41`; the list is the pull actions, `follow` and `set facing`). That zeroes `reach attacked member`, `lose aggro` taunts, the defensives and the interrupts while the tank walks to range, pulls and walks back. A member hit by a patrol or a second pack during a pull is left alone until the pull ends. The `[DEFEND]` log would show `decision` for it. Fix: end the pull (`OnPullEnded`) when `AttackedMemberRescue::FindAttacker` finds an attacker, or let the rescue and the emergency defensives through the multiplier. | S | core | read |
| 2 | **No reason is logged when a ready mark is not pulled.** In the Deadmines run 3 of 4 stalled marks have no `[PULL]` line, so either `MarkedTargetReadyTrigger` was false (which member, distance, `IsPossibleTarget`?) or the pull never started. Add one rate limited `[MARKPULL] bot mark blocker=` line (blocker: member name and reason, mark out of range, mark not a possible target, no tank strategy). No behaviour change; it is also the data source for gap 3. | S | core | read (the missing log), cause not shown |
| 3 | **The tank never says why it waits.** After about 10 s holding a ready mark, one party line naming the blocker from gap 2, once per mark, with the existing voice and cooldown rules (`BotVoice`). Owner pain: robotic, no initiative. | S | good | read |
| 4 | **Readiness is blind for the tank and the healer's sight.** `MemberReady` (`MarkTriggers.cpp`) uses 60 percent health for everyone including the tank, and 50 percent mana for everyone including the healer before a boss; it never asks that the healer sees the tank. Fix: the tank itself at 80 percent or more (named constant), the healer at 70 percent mana before a dungeon boss mark, the healer in sight of the tank (use the heal distance `AiPlayerbot.HealDistance` and `IsWithinLOSInMap`). | S | core (tank health), good (rest) | read |
| 5 | **The rescue takes the nearest mob, on foot, not the one that matters.** `FindAttacker` returns the nearest attacker of any non-tank member and `ReachAttackedMemberAction` walks to it (`Attack` then taunt). Improvements: (a) take it from range at once with the class pull or grab (Reckoning, Growl, Provoke, Dark Command, Taunt and Heroic Throw, Death Grip, Avenger's Shield) before walking; (b) rank attackers by the victim: the healer, then the weakest member, then the leader, then the rest; (c) skip an attacker that a taunt cannot change (a mob already on the other tank). | M | good | read |
| 6 | **Many loose mobs are taken one by one.** Two or more mobs on members, with only a single target taunt on an 8 s cooldown, leave the third and fourth on the member for 16 s or more. Rule: once the tank stands in reach of the group of attackers, fire the class area threat move (section 3, "Area threat") before the next single taunt; for the warrior add Mocking Banner (87) when four or more hang on members. Needs the attacker count of `FindAttacker` exposed as a value (once per call, no new scan). | M | good | inference (numbers) |
| 7 | **Loose mob order ignores casters and healers.** `IsBetter` ranks a no-aggro mob by distance only; `MarkRtiAction` marks the lowest health. A caster that stands back is reached last. Add a caster-first tiebreak using the existing "is caster" helpers, and let the tank mark a caster before a melee mob when both attack. | S | good | read |
| 8 | **Cooldowns do not know about danger.** Section 2. One shared danger reading (0 to 3) in a pure header (testable, like `ThreatMath.h` and `DefendPolicy.h`) feeding the existing class triggers; thresholds 10 points earlier at level 1, the big one at 60 percent at level 2. Also the pack-start cooldowns. | M | good | read (health-only triggers) |
| 9 | **Interrupts are not coordinated.** Several bots kick the same cast. A shared "interrupted this cast" stamp on the target (guid plus cast start time, kept in a small per-map table, no scan) lets the second kicker pick another caster or wait. The tank kicks first (it is in melee), then the others. | M | good | inference |
| 10 | **Stuns are filler.** Stun a caster when its interrupt is down; stun the mob that reached the healer when the tank cannot take it back within 2 s. | M | good | inference |
| 11 | **Bear Skull Bash is PvP only** (`BearTankDruidStrategy.cpp`; class sheet druid gap 6). | S | good | read (class sheet owns it; waits for the owner's in-game test) |
| 12 | **Standing out of bad effects covers only trigger creatures** (`AvoidAoeAction`). Dynamic-object and damaging-trap cases are not ported. Shared with every role; one position rule should own it. | M | good | read |
| 13 | **Pull path check.** A pull does not test the line to the mark for other packs (section 2). | M | good | inference (no such code found) |
| 14 | **No line of sight pull.** The pull-back spot is wherever the tank stood. A corner or pillar choice needs a navigation query. | L | expert | read |
| 15 | **No tank swap on a debuff stack or boss mechanic in dungeons** (the dungeon modules handle a few bosses; there is no shared swap rule). Brief: a second tank in the group, a stack count of the boss's tank debuff, `Taunt` on the swap. | M | expert | inference |
| 16 | **39 "All paths not in LOS" lines from the tank in one run**; the caller is not identified (flee moves, loot walk, spell-see). If it is the loot walk or a flee, the tank spent actions walking while a pull was due. | S (find) | core | read (the lines), cause not shown |
| 17 | **BT-155 has no row in the findings ledger (kept outside this repository) on `main`.** | S | docs | read |
| 18 | **The held-mark fallback is 45 s and not logged.** `HELD_MARK_FALLBACK_SECONDS` lets the others open on a mark the tank never pulled. Keep it, but log the moment it releases so a stuck tank is visible. Folded into gap 2. | S | core | read |
| 19 | **Tank mana and resource waits.** The ready test checks mana only; a tank on rage, energy or runes is never held for resources, and a tank pulled into a fight with its stance not yet up is covered by the class sheets only for the out-of-combat line. | S | good | read |
| 20 | **A mob on another floor held the group for minutes** (Stockade Hogger 7 minutes, Shadowfang Keep Frantic Geist 165 s). Fixed by BT-377: the tank and the member under attack step 12 to 16 yd away along a whole path after a give-up, the rescue taunts when the mob is in sight (`FloorAttackerPolicy.h`, `AttackedMemberRescue.cpp`). Open follow-ups: stop the walk's spline at a give-up (the tank rose 8 yd onto a ledge after one) and a ranged variant (a caster on a ledge more than 6 yd from its victim is left to the old give-up rules). | S | core | log (A1 runs), fix compile pending |
| 21 | **A bot stuck on a ledge in a player's view stayed there 9 to 11 minutes** (Ashbury wall walk). Fixed by BT-388: after 60 s one party line, then up to 3 bounded jumps to the nearest landing with a whole path to the player (`DungeonUnstuck.cpp`, `LedgeWalkOffPolicy.h`); the teleport rule is unchanged. Why bots get onto the ledge is not shown yet: the new `gave up` and `[UNSTUCK]` fields (`at`, `moving`, `rose`, `path_end_gap`, `path_end_dz`) answer it on the next run. | S | core | log (A1 runs), fix compile pending |

## 6. Top five to do first

1. **Gap 1** (S): a pull must not freeze the defend. It is a three-line change plus a test of the whitelist rule and it removes the one
   way the BT-155 fix can still go silent.
2. **Gaps 2 and 18** (S): the `[MARKPULL]` blocker log. The Deadmines run cannot answer why three marks stalled; one line answers it
   on the next run and feeds gap 3.
3. **Gap 4** (S): tank at 80 percent, healer in sight. Cheap, and it is what the owner means by "waits for the healer".
4. **Gap 5** (M): the rescue grabs from range and ranks by victim. The leader's death in Deadmines took 39 s; a 30 yd taunt in the
   first second would have changed it.
5. **Gap 8** (M): cooldowns by danger. The class thresholds are fine for a slow fight and late for a burst.

Gap 3 (the spoken reason) follows gap 2 at once; gap 6 follows gap 5.

## 7. Tests

Pure logic goes in headers under `src/Utils/` with a file in `tests/` (as `DefendPolicy.h` and `ThreatMath.h`): the
readiness thresholds (gap 4), the rescue ranking (gap 5), the danger reading (gap 8), the interrupt stamp (gap 9). Game-side
code is syntax-checked only. Live check in Deadmines with the protection paladin and a held mark: pull log lines, a second pack
walking in during a pull, a stalled mark with the blocker line, the healer at 45 percent mana.

## 8. Live test plan (Deadmines or Shadowfang Keep, finder group)

1. Leader marks skull on a pack; the healer drinks below 50 percent; the tank does not pull; after about 10 s the tank says who
   it waits for (gap 3). The `[MARKPULL]` line names the same member.
2. The tank at 70 percent health after a pull: it does not pull the next mark until 80 (gap 4).
3. During a pull walk, the leader pulls a second pack: the tank ends the pull and takes the nearest mobs, taunt first (gap 1).
4. Three mobs on the healer: the tank grabs the healer's attacker first, uses the area move, then single taunts (gaps 5, 6).
5. A caster stands back at 30 yd: the tank sends the pull tool at it before the melee mob (gap 7).
6. A boss casts a big hit: the tank's first defensive goes up before 40 percent (gap 8).

## 9. Raids (brief)

- The tank list reads `PlayerBotSpec::IsMainTank` and `GetGroupTankNum`. `TankTargetValue` already keeps the main tank on its
  current target when there are several tanks (`IsBetter`).
- The marked pull and the hold are off in raids (`HoldMarkForTank` checks `isRaidGroup`), and BT-155's rescue still applies; the
  boss exclusion (`TankHoldsBoss`) matters most there.
- Missing for raids (raids wait, owner 2026-11-08): an assignment of tanks to bosses and adds, tank swaps at debuff stacks
  (Taunt on the swap, defensive on the stack), kiting and positioning of the boss, an interrupt rotation across several
  bots (gap 9), and a cooldown timeline per boss. Only the per-boss modules under `src/strategy/dungeons/` and the raid strategy
  branches carry some of this, boss by boss.
- 25 member loops (`FindAttacker`, `MemberReady`) are cheap per call but are untested at that size; the readiness test would also
  need a raid rule (all members, or only the tank's raid subgroup).
