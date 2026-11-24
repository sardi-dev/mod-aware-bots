Approved 2026-11-24 (owner answers in section 15), implementation pending

# Temple of Kotmogu playbook

This is a proposal for a coordinated, beatable team. **Core** rules apply to every tier, **good** rules to good and expert bots, **expert** rules to experts. Proposed counts, thresholds and scores below are design choices, not observed map facts. Source paths name the core (`SkyFire_548/`) and this module.: `VOP.cpp` and `VOP.h` mean `SkyFire_548/src/server/game/Battlegrounds/Zones/BattlegroundVOP.cpp` and `.h`; `TOK.cpp` means `src/strategy/bg/BgTacticsTOK.cpp`. Patch references are in `patches/`. Sources were read on the draft date. No external guide is used: tactics here are proposals derived from the checked-in rules, not guide claims. No database was queried.

## 1. Header and goals

| Item | Fact and source |
| --- | --- |
| Map, type | Map 998 (`TOK.cpp:6`); type `BATTLEGROUND_VOP` 699 (`SkyFire_548/src/server/game/Miscellaneous/SharedDefines.h:4035`). |
| Win | First to 1600 points (`VOP.h:11`, `VOP.cpp:174-185`); otherwise the higher score at 25 minutes of play wins. On a tie the last scoring team wins; if nobody scored, the stored winner is `WINNER_NONE` (`VOP.h:92`, `VOP.cpp:61-76, 346-364`). |
| Team size and brackets | Not established from the permitted source inspection. The log reports a fill quota of 10, not the template maximum (a bot-only test run, 2026-11-19). Confirm the template and bracket data before implementation. The role tables below are proposals for common side sizes, not a claim about queue limits. |
| Preparation | Start messages include two minutes, one minute and half a minute (`VOP.cpp:42-45`); doors visually open six seconds early, collision opens at start (`VOP.h:96`, `VOP.cpp:48-56, 121-129`; patch `0021-Add-the-start-room-collisions-of-Temple-of-Kotmogu.patch`). |
| Goal | Take available orbs, keep carriers alive in the highest sustainable scoring zone, deny enemy carriers and avoid feeding kills. Reassess on each pickup, loss and major fight. |
| Authority | Core and checked-in patches decide mechanics. `0020-Rework-the-Temple-of-Kotmogu-rules.patch:40-56, 653-664, 781-815` introduces the rework; the checked-out core contains it. Deployment of the SQL and actual runtime revision are unverified. ADR `docs/decisions/0032-*.md` specifies leader, tier mix, human call-outs as information and deterministic fallback. |

## 2. Map and positions

Coordinates are x, y, z in map units; orb coordinates and ids are from `VOP.cpp:23-29`, `VOP.h:60-68`. Names west and east below follow the bot file's x-axis convention, not a claim about compass orientation (`TOK.cpp:75-78`).

| Orb | Object id | Pedestal x, y, z | Pickup aura |
| --- | --- | --- | --- |
| Blue | 212091 | 1716.83, 1249.93, 13.5706 | 121164 |
| Purple | 212092 | 1850.26, 1416.77, 13.5709 | 121175 |
| Green | 212093 | 1716.78, 1416.64, 13.5709 | 121176 |
| Orange | 212094 | 1850.29, 1250.31, 13.5708 | 121177 |

The mapping follows ordered entries and ordered aura array (`VOP.h:14-24, 60-68, 99-106`; `VOP.cpp:216-224, 227-243`). The bot's proposed centre waypoint is (1783.5, 1333.0, 5.708), a navmesh point, not a verified scoring boundary (`TOK.cpp:55-60`). Use the core trigger geometry to verify actual positions before choosing posts.

Alliance and Horde doors are respectively (1783.84, 1100.66, 20.60) and (1780.15, 1570.22, 24.59), door object 213172, collision 215787 (`VOP.cpp:17-21, 282-290`; `VOP.h:63-64`). Field spirit guides spawn at (1892.61, 1151.69, 14.7160) for Alliance and (1672.40, 1524.10, 16.7387) for Horde (`VOP.cpp:296-297`); selected field safe locations are 3552 and 3553, and pre-start safe locations 4058 and 4057 respectively (`VOP.cpp:411-432`). Safe-location coordinates and paths from them are not verified.

Two buff objects use `Buff_Entries[2]` at (1856.007935, 1333.637392, 10.554197) and (1710.443604, 1333.375000, 10.554073) (`VOP.cpp:303-305`). Their actual spawned type and any random replacement need a data and live check; do not assume a specific buff from this call alone. The raised approaches, pedestal edges and centre pit need navigation and line-of-sight checks, not invented routes.

## 3. Mechanics that matter for play

- **Scoring:** each held orb has its own five-second timer, reset on pickup and loss. Each tick gives its carrier's team 3 outside the ring, 4 inside the middle ring, or 5 inside both ring and centre (`VOP.h:92-94`; `VOP.cpp:79-101, 148-171, 227-240, 260-264`). The centre is defined by area trigger 7735 nested inside 7734 (`VOP.h:84-88`). The core rotates x and y into each DBC box and checks horizontal extents in order; it does not test z (`VOP.cpp:149-169`). Exact DBC dimensions, orientation and usable vertical surfaces are unverified. If a trigger lookup fails, the loop breaks, reducing the possible award (`VOP.cpp:152-157`).
- **Kills:** a non-self killer receives 10 team points on a player kill, whether either player carries an orb; the victim's orb returns first (`VOP.h:94`; `VOP.cpp:375-388`). Protecting carriers and avoiding deaths both matter.
- **Pickup and return:** an in-progress click within 10 units takes an available orb, with at most one per player; the object disappears and its availability state changes (`VOP.cpp:208-249`). Death, removal from the battleground or removal of the pickup aura releases it; it respawns immediately at its original pedestal, not on the corpse (`VOP.cpp:132-137, 253-279, 375-380`; `SkyFire_548/src/server/scripts/Pandaria/Battlegrounds/temple_of_kotmogu.cpp:35-42, 55-58`). A teammate near the death location cannot simply pick the dropped orb up there.
- **Carrier effects:** the aura script adds each effect's base amount again to its damage, healing and damage-taken effects on each aura periodic tick, and adds 5 to its scale effect (`temple_of_kotmogu.cpp:24-28, 44-58`; patch `0020:781-815`). Base amounts, signs, exact tick interval, limits and spell restrictions depend on spell data and are not established here. Do not equate this aura tick with the separate scoring tick. SQL binds the four spells to this script (`0020:50-56`); confirm it is applied live.
- Score is capped at 1600; a tie at time uses last scorer, including a kill (`VOP.cpp:187-205, 68-75, 384-388`; `VOP.h:11`). Premature winner uses score first and falls back to the base implementation on a tie (`VOP.cpp:456-463`).

## 4. Roles by side size

These are proposed allocations, counting actual living participants, including humans, without controlling humans. A carrier is a state, not a permanent slot. Prefer mobile, durable damage players for pickup; keep healers available to heal unless the free orb would otherwise be lost. Rank candidates by reachable distance, survival, healer availability and current assignment, with stable GUID tie-breaks. Never rely on unavailable class abilities.

| Available side | Opening jobs | After pickup |
| --- | --- | --- |
| Small, about 5 | 2 pickup runners on opposite approaches, 1 support, 1 hunter, 1 flexible | Support the most threatened carrier; a runner whose orb is taken switches to hunting or support. |
| Full, about 10 | 4 pickup runners split across sides, 3 support including available healers, 2 hunters, 1 flexible | Carriers count within the team, not in addition. Support nearest threatened carriers, hunters attack seen enemy carriers, flexible player covers a free pedestal or supports the weaker fight. |

At other sizes scale the same priorities, reserving at least one support if the side has a carrier and at least one pickup candidate if an orb is free. Do not assign more than one bot as primary to the same pedestal unless a visible fight warrants support. On death, aura loss, a human pickup, or a failed path, release the reservation and reassign the nearest suitable alive bot. If all orbs are held, pickup runners become escorts or hunters; do not wait at empty stands.

## 5. Opening plan

**Core:** wait behind the collision until the match starts. From either faction's door, choose reachable near-side pedestals, split runners across distinct free orbs, and send support toward the centre approach. A pickup runner interacts and moves toward a supported scoring position; others meet the carrier en route. A free orb restored to its pedestal becomes a new objective immediately. The symmetric plan uses actual distance and path, not a hard-coded faction ownership of orbs.

**Good:** contest a threatened pickup with an escort, and interrupt enemy healers or peel rather than everyone fighting at one pedestal. If a human reaches an orb first, support that carrier. **Expert:** delay a risky solo pickup briefly for nearby support, unless the enemy is about to take it. Reassess once the initial encounters resolve; there is no permanent opening split.

## 6. Mid-game rules

1. **Core:** take a reachable free orb, support friendly carriers, and attack a visible enemy carrier. Do not chase a player away from an objective indefinitely. A kill returns the enemy orb to its pedestal, so reserve a runner for that pedestal rather than chasing the corpse.
2. **Core:** a supported carrier enters the centre when safe; if under severe pressure or isolated, use the ring or outer cover while moving toward friendly support. Surviving at a lower rate can beat dying and surrendering both the orb and kill points. Avoid repeatedly crossing a boundary for negligible benefit.
3. **Good:** use a proposed health threshold of 40 percent or visible local disadvantage as a retreat signal, and return when support arrives and health recovers above 65 percent. Require a short dwell before changing zone except for immediate danger. Use health, nearby visible threats, support and current aura amount, not an assumed stack timer.
4. **Good:** when ahead and projected to win, protect carriers and avoid unnecessary risks; when behind and the remaining time makes passive scoring insufficient, send a larger hunting group and attempt a centre hold. Recalculate from current scores, observed rates and clock. Do not declare a win from an extrapolation that ignores kills or lost orbs.
5. **Expert:** coordinate a burst on a vulnerable visible enemy carrier and control its healer, while leaving a viable escort. When wiped, regroup on a safe approach and assign a pickup rather than repeatedly entering alone. Never intentionally drop an orb to reset the aura without owner approval and verified mechanics.

## 7. Composition-dependent choices

With multiple healers distribute them across carrier groups; a spare healer follows the most threatened fight. With one healer, keep that player near the main carrier group and avoid a split that strands them. With none, use cover and short, supported holds rather than relying on centre healing. Stealth damage players scout or contest a pedestal but cannot pass unseen intelligence magically to teammates. Ranged players pressure from supportable line of sight, melee peel or burst with the hunt group. Do not assume a tank specialization is available or required (ADR `0032`, decision 12).

## 8. Call-outs and the leader

One bot leader selects the plan; a human is never assigned as bot leader. Choose best tier, then level, then stable GUID tie-break, following the existing approved example (`docs/bg-sheets/twin-peaks.md:417-418, 422`). Propose short messages such as `take blue`, `help green carrier`, `enemy orange mid`, `blue returned`, `regroup mid`, `push their carrier`. Pickup and return announcements from the battleground are separate from bot chat.

Only report an enemy personally seen or conveyed by an actual visible battleground call-out; use map-provided public objective states only for what they reveal. Never share exact enemy position or health from a global scan. Human chat is information, not an order; parse recognized colour, location, `inc`, `help` and count conservatively and expire reports. Proposed suppression: one repeated equivalent call per 20 seconds per side, with a distinct urgent loss allowed sooner; rate limits and expiry are tuneable. Expert bots call more and follow promptly; casuals react late and can drift. Avoid exposing a human's hidden information via bot chat.

## 9. Class jobs on this map

| Class | Proposed job, conditional on known spells and spec |
| --- | --- |
| Warrior | Peel with control, pressure a seen carrier, use mobility to recover a pedestal. |
| Paladin | Heal and protect carriers, or support a pickup with defensive tools; avoid an immunity that releases an orb until verified. |
| Hunter | Track visible threats, slow attackers and pressure enemy healers from a supportable position. |
| Rogue | Contest pedestal approaches, control a healer and join a coordinated carrier burst. |
| Priest | Heal or pressure, dispel valid effects and protect carrier line of sight. |
| Death knight | Disrupt enemy movement and peel for the carrier; use pulls only when legal and useful. |
| Shaman | Heal, interrupt and slow attackers around the support group. |
| Mage | Control enemy support, slow a carrier and use escape tools without abandoning the objective. |
| Warlock | Pressure and control from cover; place mobility tools where the group can still support the user. |
| Monk | Mobile healing or peel, assist recovery and maintain line of sight. |
| Druid | Mobile healing or pickup when appropriate; check form restrictions before interacting. |

These are tactical assignments, not guarantees that particular spells work with an orb. Check spell availability and battleground restrictions per bracket.

## 10. Failure modes

- **Wrong pedestal selected:** verify colour to aura mapping and spawned object before reserving, release failed reservations, and replan.
- **Carrier dives alone or leaves healing range:** use visible support and retreat thresholds; measure centre time and survival rather than forcing the pit.
- **Entire team chases one target:** cap the hunt allocation and retain pickup and escort coverage.
- **Orb returns to a distant pedestal:** send a runner to its actual stand after the event, not to the death site.
- **Repeated pedestal or pit path failure:** detect lack of progress, clear the objective and retry from a safe approach; log coordinates. Check raised edges, stairs, line of sight, field graveyards and start collision live before adding route points.
- **Enemy location leaks:** do not read an unseen carrier's position from the roster; use sight or a current visible call-out, and expire stale data.
- **Low-health carrier feeds kill points:** peel or withdraw to support; do not sacrifice the orb solely for a higher tick.
- **Unverified zone boundary:** test actual awards at boundaries and height before treating a waypoint as centre.

## 11. Skill tiers

Core: interact with free orbs, protect a carrier, pursue seen objectives, return from graveyard and respect game rules. Good: threat-aware positioning, role replacement and coordinated pursuit. Expert: read aura risk, coordinate control and burst, anticipate a returned pedestal and change zone with hysteresis. Casuals follow core rules with delayed reactions and some temporary role drift, not fabricated knowledge. Target mix approximately 10 percent expert, 60 percent good and 30 percent casual per the sheet method; the PvP design decision targets 75, 55 and 35 percent wins against an average human by tier, requiring measurement rather than a guarantee.

## 12. Bots today and gaps

| Topic | Evidence and assessment |
| --- | --- |
| Start and pickup | Waiting spots and randomized offsets in `TOK.cpp:62-71, 243-251`; orb interaction, mount removal and direct core click in `TOK.cpp:399-459`. Existing pickup actions log an attempt before the core confirms it. |
| Plan | Six weighted team strategies (`BattleGroundTactics.cpp:1781-1800`) and fixed role-rank allocations (`TOK.cpp:75-116`); supports orb, centre and hunt groups, but no evidence in this map file of a leader updating allocation for score, composition or threat. |
| Carrier | Always selects a centre waypoint (`TOK.cpp:269-277`); no threat-aware retreat in this branch. Shared carrier detection includes orb auras (`BattleGroundTactics.cpp:4869-4884`). |
| Enemy intel | `selectObjectiveTOK` scans all battleground players and reads auras and exact coordinates without visibility filtering (`TOK.cpp:296-334, 370-385`). This can expose unseen enemy carriers and conflicts with seen-only policy. |
| Wrong orb identity | The holder aura array is ordered green, purple, orange, blue while the stand arrays are blue, purple, green, orange (`TOK.cpp:50-52, 296-314`). Therefore green, orange and blue occupancy is attributed to the wrong stand; purple aligns. This is a code inference, requiring a targeted test. |
| Scoring sync | `TokSyncZone` is compiled out (`TOK.cpp:168-172, 228-229, 405-407`). Its older comments describe trigger toggles, but checked-out core uses direct position tests (`VOP.cpp:148-171`); disabled sync alone is not proof of broken scoring. |
| Live evidence | A bot-only test run log (2026-11-19) records plans and attempts at each colour, not confirmed core pickup or scoring. The excerpt ends during play , so no match result, scoring rate or current deployment can be inferred. |

## 13. Gap list and proposed changes, ordered

1. **Correct occupancy mapping and visibility** in `BgTacticsTOK.cpp`, using ordered colour data and seen-only enemy observations. High priority, medium scope and risk: test all colours and hidden carriers; verify public objective information separately.
2. **Create a per-side leader plan** using shared leader and call-out components and a map-specific state adapter, rather than fixed per-match ranks alone. Medium to large scope, high coordination risk: release stale reservations on loss, death, human interaction and leader replacement.
3. **Implement carrier, escort and recovery decisions** in the map tactics: reachable scoring posts, support and hysteresis, plus return-to-pedestal behavior. Medium scope, navigation and balance risk. Preserve generic combat and avoid overriding humans.
4. **Add score, time and call-out handling** through existing shared interfaces, with expiring reports and per-tier response; medium scope, risk of spam or unfair information.
5. **Instrument and validate the patched core and data**, especially score zones, aura script binding and pickup success. Small instrumentation proposal, operational risk if applied to production; no core change is authorized by this draft.
6. **Tune routes and thresholds after a live human queue**, not from straight-line estimates. Medium scope, map and class dependent.

## 14. Tests

Do not run tests as part of this draft. Before implementation, verify template, brackets, trigger records, spell data and active patch/SQL by authorized read-only inspection. Then use pure tests with a fake observed state: all four aura-to-stand pairs, independent timers, pickup reservations, human pickup, loss and immediate pedestal return, death, stale or unseen reports, score/time transitions, deterministic candidate ordering, tier delay, leader replacement and no eligible carrier. Include zero-score timeout and missing trigger records.

In an authorized scenario harness if it supports this map, exercise outside, ring and centre positions with identical bot and human carriers and compare awarded score to core rules; test horizontal boundary and different heights, simultaneous carriers, kill points, aura periodic changes, aura removal, logout and duplicate clicks. Verify the script binding and real spell restrictions before deciding whether voluntary drop can be supported.

Run a human queue with the headless client and appropriate seeded participants only with the owner's normal test authorization. Record score and end reason, pickups confirmed by state, time held and scoring zone per orb, deaths and kill points, role assignments, call-outs, recovery latency, route failures and sight provenance. Acceptance: match finishes, both sides seek and defend objectives, no unseen enemy coordinates drive bots, no repeated stuck loops or chat flood, and a human team can both win and lose across representative matches. One log or one match cannot establish tier win rates.

## 15. Owner questions (recommended option first)

1. **Carrier positioning:** (a) Recommended: centre when supported, retreat to a safer scoring zone on observed danger, and return with hysteresis. (b) Always centre. (c) Always stay outside.
2. **Intentional aura reset:** (a) Recommended: do not deliberately drop; investigate actual aura data and restrictions first. (b) Permit a coordinated release and pedestal recovery after validation. (c) Release at a fixed assumed stack count.
3. **Opening and full side allocation:** (a) Recommended: four distinct pickup candidates, three support, two hunters and one flexible for a proposed full side; scale down and account for humans. (b) Keep existing weighted random splits. (c) Concentrate on centre first.
4. **Information policy:** (a) Recommended: replace global enemy carrier position and aura scans with personal sight and expiring visible call-outs, retaining only genuinely public objective state. (b) Keep the global scan. Confirm which carrier-position UI updates count as public information before using them tactically.
5. **Orb-rule deployment and values:** (a) Recommended: retain the checked-in rework for the playbook and verify deployed core, SQL bindings, trigger and spell data before any fork change. (b) Revisit the rules against independent era evidence first. No guide versus core disagreement is established in this draft.
6. **Testing:** (a) Recommended: pure scenarios first, then a human-queue test with seeded participants if supported. (b) Wait for natural queues. Confirm permitted test accounts and applicable bracket before live work.

**Owner answers (2026-11-24):**
- 1 (a): carriers go to the center when supported, retreat on seen danger and come back with hysteresis.
- 3 (a): opening 4 pickups / 3 support / 2 hunters / 1 flexible for a full side, scaled down for smaller sides and humans.
- 4 (a): seen-only intel for enemy carriers; public map state stays allowed.
- 2 (a) and 6 (a): no deliberate drops until the aura data is checked; pure tests first, then a seeded bot-only
  match, then a human queue test with the owner.
- 5: no fork change is proposed; the deployed rework is verified during the live test.

## 16. Decision points for the leader bot

Proposed pure candidate lists use only public scores and clock, friendly state, personally seen enemy state and unexpired visible call-outs. Ask every 15 to 30 seconds and on meaningful objective or leader events, with a rate limit; no free-form coordinates. Highest integer score wins, ties by fixed candidate id; when the decision layer is absent, times out, or returns an invalid id, use that first candidate. Numbers in this table are proposed policy weights and thresholds, not core facts. Local combat remains scripted.

| Decision | Candidates and deterministic fallback scores |
| --- | --- |
| Stance | `protect` 100 if ahead and projected to win; otherwise 40. `recover` 100 if behind and projection loses; otherwise 50. `balanced` 80 otherwise, 30 in either decisive condition. Projection includes uncertainty and never guarantees an outcome. |
| Free orb | For each known free colour, `assign:<colour>` scores 100 minus capped reachable-distance rank, plus 10 if enemy contest is seen; `none` scores 0. Only eligible unassigned candidates; tie by colour then stable player id. |
| Carrier zone | `centre` 100 when supported and safe, else 30; `ring` 100 under pressure with a reachable escape, else 50; `outer` 100 for immediate danger without ring safety, else 20. In overlapping conditions choose outer for immediate danger, then ring for pressure, else centre; assign the chosen candidate 100 and the others 30, 50 or 20 as above. Retain previous zone during dwell unless immediate danger. |
| Hunt | `hunt_seen_carrier` 100 if a carrier is seen and escort remains viable; `escort` 100 if friendly carrier is in immediate danger; `regroup` 100 if no viable group remains. Resolve simultaneous conditions in that priority order: escort, regroup, hunt; other eligible candidates score 40. |
| Returned orb | `recover_pedestal` 100 if the return is known and an eligible runner can reach it; `support` 80 if no runner is available; `regroup` 60 otherwise. No enemy corpse location is used as the pedestal. |

Decision inputs must include observation timestamps and provenance, alive and assigned friendly players, known orb availability, score, time, local support and path feasibility. Unknown enemy status stays unknown; it is not interpreted as safe. Thresholds are proposals to tune after measurement.

## 17. Core and data findings

- The present core is `BattlegroundVOP`, not a `BattlegroundTK` file. Patch `0020` replaces team-count ticks with independent carrier scoring, changes timeout and implements aura handling (`0020:653-674, 781-815`). Patch `0021` adds start collision; `0022-Send-the-positions-of-flag-and-orb-carriers.patch` supplies carrier position reporting. Reporting does not by itself authorize omniscient bot decisions.
- The score zone implementation checks only horizontal DBC box dimensions, not height (`VOP.cpp:159-169`). This may award centre rate on an upper surface inside the horizontal footprint; whether such a reachable surface exists is unverified. Also check missing or malformed DBC records and the order of the boxes. This is a finding to test, not a proposed silent core fix.
- Orb aura periodic magnitude and frequency are not contained in the script itself. The SQL file is present, but live database state, client data, exact spell restrictions, brackets, template team limits, world-object duplicates and safe-location coordinates were not inspected. The user prohibited database queries; no values are invented.
- The log is partial and may come from another runtime revision. It establishes attempts and role labels only. No build, server, database query, download, install or commit was performed for this sheet.
