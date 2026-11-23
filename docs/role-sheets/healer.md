# Role sheet: healer (all six healing specialisations)

- **Status:** draft of 2026-11-12, written with the first implementation (BT-157, compile pending, not tested live, not
  merged). Facts about the code were checked by reading the call sites at `main` (commit b518acb) unless a line says
  "inference". Levels and costs are from the class sheets (`docs/class-sheets/`), which carry the 5.4.8 data.
- **Owner direction (2026-11-12):** bot healers are too passive: "they do a little of actions only, sometimes just
  standing on the same place until somebody takes damage". A priest keeps shields on the tank at least, with mana
  guardrails; healers in general are proactive. Guardrails: shield and a heal over time on the tank before and during
  every pull while mana is above 30 percent; on other members only above 60 percent mana or when they take damage;
  below 30 percent only reactive heals, and the healer asks to drink; stay near the group with sight on the tank. Same
  day: a priest raised a bot but never a player, who lay dead a minute and ran back.
- **Skill tiers (design decision 0017, 10 percent expert, 60 good, 30 casual):** every rule below is marked **core** (all tiers),
  **good** (good and expert) or **expert**.
- **Scope:** five-player dungeons first, raids brief at the end.

## 1. What a good healer does

### Between pulls
- Stays where it has sight of the tank and is inside its heal range of him (core). Never sits far behind a wall.
- Drinks below 50 percent mana in a dungeon (core, `DungeonDrinkMana`), asks the group to wait in chat (existing
  `DungeonChat` DRINK line). A tank bot pulls a mark only when the whole group is ready, healer mana at 50 percent
  included (`MarkTriggers.cpp` `MemberReady`), so the healer's drink is what the group waits for.
- Tops up anyone below 85 percent with the cheap heal over time, then drinks (class sheets, core).
- Raises dead members, real players first (section 5, core).

### At the pull
- The tank is about to pull when the leader's mark, or the tank's own target, is a living hostile that is not fighting,
  within 40 yards of the tank. The healer prepares him before the mob arrives:
  1. first spell (**core**): Power Word: Shield (disc), Renew (holy), Riptide (restoration shaman), Rejuvenation
     (druid), Renewing Mist (monk), Beacon of Light (paladin);
  2. second spell (**good**): Renew (disc), Earth Shield, Lifebloom, Sacred Shield (if talented);
  3. third (**expert**): Prayer of Mending (priest, 68), Enveloping Mist (monk, 3 Chi).
- Needs more than 30 percent mana. The preparation of one pull target lasts 20 seconds at most (a group that drinks
  must not make the healer renew every 15 seconds). A bot tank's mark counts only when the whole group is ready; a
  human tank's selected target counts at once.

### In the fight
- Triage (unchanged, class sheets): critical member first, then the tank, then the rest, lowest first.
- Tank upkeep continues while mana is above 30 percent (core): the shield whenever Weakened Soul is gone, the heal over
  time whenever it is missing, Lifebloom to three stacks, Earth Shield and Beacon kept up.
- Members other than the tank:
  - above 60 percent mana: a heal over time on the heal target when he is below 98 percent (**good**);
  - 30 to 60 percent: only on a member whose health fell in the last 6 seconds (**expert**);
  - 30 percent or below: nothing proactive. Reactive heal chains run as before; the shared save-mana rule still holds
    back inefficient top-ups below 60 percent.
- At 30 percent or below, after 15 seconds of combat, the healer says it is low on mana (existing LOW_MANA line, was
  25 percent). Mana cooldowns (Shadowfiend, Mana Tea, Innervate, Mana Tide, Divine Plea) stay as in the class sheets.
- Walks back to the tank when it lost sight of him or stands beyond its heal range (**core**), by path, around corners.

### After the fight
- Top up with heals over time, raise the dead, drink, then the next pull.

## 2. Spells that fill each job (level, source: class sheets)

| Job | Disc priest | Holy priest | Resto shaman | Resto druid | Mistweaver monk | Holy paladin |
|-----|-------------|-------------|--------------|-------------|-----------------|--------------|
| Pre-pull 1 (core) | Power Word: Shield (5) | Renew (26) | Riptide (10) | Rejuvenation (4) | Renewing Mist (42) | Beacon of Light (39) |
| Pre-pull 2 (good) | Renew (26) | Prayer of Mending (68, expert) | Earth Shield (26) | Lifebloom (36) | Enveloping Mist (16, 3 Chi, expert) | Sacred Shield (talent) |
| Proactive top-up | Renew | Renew | Riptide | Rejuvenation | Renewing Mist | Holy Shock (10, 8 percent mana, no heal over time exists) |
| Ask to drink | chat, shared | chat, shared | chat, shared | chat, shared | chat, shared | chat, shared |

Monk note: Enveloping Mist costs 3 Chi and no mana; before level 42 the mistweaver has no heal over time and no Chi source
the bot uses, so the pre-pull line is silent there (class sheet BT-135).

## 3. Thresholds and where they live

| Rule | Value | Where |
|------|-------|-------|
| Tank upkeep needs mana above | 30 percent | `Utils/HealerGuardrails.h` `REACTIVE_ONLY_MANA_PCT`, gate `HealerUpkeep::TankUpkeepAllowed` |
| Top-up of any hurt member above | 60 percent | `PROACTIVE_MEMBER_MANA_PCT` |
| Recent damage window | 6 s | `RECENT_DAMAGE_WINDOW_MS` |
| Member counts as hurt below | 98 percent | `PROACTIVE_HEALTH_CEILING_PCT` |
| Pre-pull window per target | 20 s | `PRE_PULL_WINDOW_MS` |
| Pull target within | 40 yd of the tank | `PRE_PULL_TARGET_RANGE` |
| Walk back to the tank beyond | heal distance 25 yd, or no sight | `HealerTankToReachValue`, `AiPlayerbot.HealDistance` |
| Rejoin walk: no member within, then for | 40 yd, 30 s (beyond sight distance, or still 20 s) | `Utils/GroupRejoinPolicy.h`, `StuckMemberPolicy.h` |
| Rejoin drop of a stale combat flag | two 10 s windows in which the bot moved under 5 yd and gained under 5 yd on the tank, no damage (core hook) for 30 s, no attacker within 6 yd, no player or player pet attacking; then `CombatStop` plus `deleteReferences` | `GroupRejoinPolicy.h` `NextStep`, `FlagIsStale` |
| In-combat "low on mana" line at or below | 30 percent | `DungeonChat.cpp` `WatchMana` via `ShouldAskToDrink` |

## 4. What the bots do today (before BT-157) and the gap list

State at `main`, with file and function. "Done" means implemented in the scratch copy for BT-157, not merged.

| # | Gap | Size | Tier | State |
|---|-----|------|------|-------|
| 1 | No tank upkeep before the pull: no healer strategy for the non-combat engine had a tank shield or heal over time node, so the first hit landed on an unprotected tank (`PriestNonCombatStrategy.cpp`, `GenericDruidNonCombatStrategy.cpp`, `ShamanNonCombatStrategy.cpp`, `GenericPaladinNonCombatStrategy.cpp`, `MonkStrategies.cpp` `GenericMonkNonCombatStrategy`) | M | core/good/expert | done: nodes plus value `healer pre pull` (`value/HealerValues.cpp`) |
| 2 | Tank upkeep needed mana above 60 percent (`PriestTriggers.cpp` Renew, Prayer of Mending, Power Word: Shield; `DruidTriggers.cpp` Rejuvenation; `MonkActions.cpp` Renewing Mist), so a healer at 40 to 60 percent mana stopped shielding the tank | S | core | done: guardrail 30 percent |
| 3 | Lifebloom, Earth Shield, Beacon of Light and Sacred Shield had no mana rule at all (`DruidTriggers.cpp` `LifebloomOnMainTankTrigger`, `ShamanTriggers.h`, `PaladinTriggers.h`) | S | core | done: same gate |
| 4 | Riptide was never kept on the tank (`HealShamanStrategy.cpp` had Earth Shield only) | S | core | done: `riptide on main tank` |
| 5 | A member 85 to 98 percent health got nothing, so a healer with full mana stood still until someone fell below 85 percent | M | good/expert | done: `healer proactive target` value, `* proactive` triggers and `CastHealerProactiveHotAction` |
| 6 | Healer did not walk to a tank it could not see unless the tank was hurt: `ReachPartyMemberToHealAction::FindHurtMemberOutOfSight` (BT-117, BT-155) only looks at members below the medium health threshold | M | core | done: `healer tank to reach` value, `reach main tank to heal` (path walk, same as BT-155) |
| 7 | The in-combat "low on mana" line fired at 25 percent, the healer's own reactive-only line is 30 | S | core | done |
| 8 | Dead real players were not raised: section 5 | M | core | done |
| 9 | Resurrection of a ghost whose body is far from the ghost: the cast targeted the ghost unit, which the core wants in range and sight of the caster | M | core | done (owner yes 2026-11-12): the cast targets the corpse (`PlayerbotAI::CastSpellOnCorpse`, `ResurrectPartyMemberAction::Execute`). Needs the core change below to work for a ghost out of sight |
| 10 | "Took damage" is read from the heal target only (the value already computed), so a hit on a member that is not the lowest is not seen | S | expert | open: a damage hook like the one `BotScenario.cpp` uses would see every hit |
| 11 | Pain Suppression, Guardian Spirit, Life Cocoon on the tank by incoming damage, not only by health (BT-129 covers the protect value) | M | good | done as BT-293 (2026-11-16, compile pending): "main tank in danger" reads the bot tank's danger level (BT-292) and fires Pain Suppression, Guardian Spirit, Ironbark or Life Cocoon at 50 percent (dangerous) or 70 (deadly). Open: holy paladin Hand of Sacrifice has no action |
| 12 | Healer positioning against ground effects and cleaves (stands where the follow code left it, 8 to 25 yd from the tank) | L | good | partly done as BT-294 (ground effects from dynamic objects); BT-291 ends the pre-pull sight walk where she gains sight instead of on the tank. Cleaves open |
| 17 | Dispels outrank every heal (ACTION_DISPEL 50 against critical heals 30) | S | core | done as BT-295 (2026-11-16, compile pending): dispels wait while a member the healer can heal now is under 35 percent; a dispel on the tank never waits |
| 13 | Out-of-combat top-ups still use expensive heals before a drink (Flash Heal, Penance in `PriestNonCombatStrategy.cpp`; Surging Mist) | S | good | open (class sheets priest-discipline 12, monk-mistweaver 21) |
| 14 | Group mana cooldowns used for the group, not only for self (Hymn of Hope, Mana Tide Totem, Innervate on another healer) | M | expert | open |
| 15 | Pre-pull waits for a bot tank's group-ready test, but a human tank's pull is read from his selected target only | S | core | known limit |
| 16 | A healer far from the group in a stale fight had no owner: the heal code skips a tank beyond sight distance, the follow code does not run while flagged (Shadowfang Keep run 7, 20 min at one spot, tank died 108 yd away) | M | core | done on `fix/healer-far-from-group` (BT-201 names the state, BT-202 walks to the tank and drops a stale flag; `strategy/generic/RejoinGroup.cpp`, `Utils/GroupRejoinPolicy.h`). Live test pending |

## 5. Resurrection (owner report 2026-11-12)

**Reading.** `PartyMemberToResurrect::Calculate` (`value/PartyMemberToResurectValue.cpp`) took the first group member in
the order leader, healers, tanks, rest whose death state is CORPSE, with no resurrect request open and no group member
already casting a resurrection, and that passed `PartyMemberValue::Check`: same map, distance under twice the spell
distance (50 yd) and line of sight from the bot to the body. Why a human is skipped, in order of likelihood:

1. **Out of sight or out of range (read, strong).** A player who died around a corner, down a ramp or ahead of the group
   fails `Check`. The trigger `party member dead` is `GetTarget()` of that value, so no trigger fires, and the walk
   (`reach party member to resurrect`) is only a prerequisite of a cast on a found target. Nobody ever walks to such a body.
   The BT-155 diagnosis shows the same shape for hurt members (the leader died ahead of the group).
2. **A released player is a ghost far from the body (read).** A released player stays in death state CORPSE (the core
   sets CORPSE in `Player::KillPlayer`), but the ghost stands at the graveyard, usually outside the dungeon or beyond 50
   yd. The unit fails `Check` for the same reason.
3. **An open request is never closed (read in the core).** `Spell::EffectResurrect` returns silently when
   `IsRessurectRequested()`; `ClearResurrectRequestData` runs only on the player's answer (`HandleResurrectResponseOpcode`: a decline, or an accept),
   on a new death and when the player object is destroyed (logout). A request from an earlier cast (a bot raised him, he did not answer, or answered after the
   dialog was gone) stays, the value skips him for good (`!player->IsRessurectRequested()`), and any new cast would be
   ignored. A core change that lets the newest request win with a 60 s expiry is being made separately.
4. **Role order, not human order (read).** The old order put a dead bot healer or tank before a dead friend; it never
   preferred a bot over a human on purpose, but a human who is neither leader nor a tank sat behind every bot.
5. Not a cause: the death state of an unreleased player (CORPSE, same as a ghost), the combat resurrection edges (they are
   combat-only for druid and death knight; the other healers have the out-of-combat edge `party member dead` in the
   non-combat engine).

**Fix (BT-157).** `Utils/ResurrectPolicy.h` ranks the candidates: eligible = no open request, nobody casting on him,
within 100 yd on the same map; order = real player first, then in sight, then healer before tank before rest, then nearer.
`ReachPartyMemberToResurrectAction` walks by path (`MoveTo`) to the corpse of a released player, or to the body, out of
combat; in combat it keeps the shared reach. `ResurrectPartyMemberAction::isUseful` refuses a target with an open request
(Rebirth and Raise Ally now call it too). Tier: **core**.

**Corpse target (gap 9).** For a released player whose ghost is more than 10 yd from its body, or on another map, the spell is
cast at the corpse: target mask `TARGET_FLAG_CORPSE_ALLY` and the corpse GUID, the same two fields the client sends
(`SpellCastTargets` constructor, `SpellCastTargets::Update` finds the corpse again from the GUID). The value lists such a
member by the corpse's position and sight. The core measures no range for a corpse target (`Spell::CheckRange` reads the unit
target only), so `ResurrectPartyMemberAction::Execute` keeps the bot inside the spell's maximum range less 3 yd and in sight of
the corpse, and the shared reach action walks it there first.

**Core finding (needs the separate core change).** Every 5.4.8 resurrection of the healer classes (Resurrection 2006,
Redemption 7328, Revive 50769, Resuscitate 115178) has effect 18, `SPELL_EFFECT_RESURRECT` (read from `SpellEffect.dbc`). For a
corpse target the core resolves the owner (`Spell::SelectEffectTypeImplicitTargets`, the "workaround" that adds the owner),
and then `Spell::CheckEffectTarget` drops the owner when he is not in line of sight of the caster. Only
`SPELL_EFFECT_RESURRECT_NEW` (effect 113) has the branch "player far away, maybe his corpse near?" that accepts the corpse
instead. So a ghost at a graveyard is never raised through its corpse, by a bot or by a player, until
`case SPELL_EFFECT_RESURRECT:` joins `case SPELL_EFFECT_RESURRECT_NEW:` in `CheckEffectTarget` (`Spells/Spell.cpp`, about line
7499). Until then the corpse cast fails for a far ghost, backs off one second, and the bot walks and retries.

**Still open.** A body in the middle of a live pack: the walk starts only out of combat, but mobs that have not engaged are
not checked. A range check for corpse targets in the core.

## 6. Raids (brief)
- Same upkeep, with the raid tank list (main tank flag) instead of the party's single tank; the value reads
  `PlayerBotSpec::IsMainTank`.
- Raids need healer assignments (tank healers, raid healers), a dispel assignment and a cooldown timeline per boss; none
  exist (raids wait, owner 2026-11-08). The 25 member loops (`FindMainTank`, `DamageWindow` with 8 slots) are cheap but
  untested at that size.

## 7. Tests
`tests/test_healer_guardrails.cpp` (mana bands, pull imminence, pre-pull window, damage window) and
`tests/test_resurrect_policy.cpp` (eligibility and order). `tests/test_stuck_member.cpp` and `tests/test_group_rejoin.cpp` (far clocks, walk windows, stale flag). `bash tests/run.sh`: 395 tests, 0 failed. Game-side code is
syntax-checked only (g++ `-fsyntax-only`), not built or run.

## 8. Live test plan (Deadmines, priest healer)
See the hand-off report; summary: run it with the disc priest healer and a bot tank, with a debug log of the action
queue, and check, in order: (1) shield and Renew on the tank before the mob arrives, once per pull, none while the group
drinks; (2) shield re-cast every 15 s in the fight; (3) at 40 percent mana the shield continues, Renew-on-others stops
unless a member was hit; (4) at 28 percent the "low on mana" line appears and only reactive heals run; (5) the healer
steps back into sight when the tank goes around a corner; (6) the owner dies out of sight: the priest walks to the body and
raises him once; (7) no second cast while the request is open.
