# Team play: the healer lock, the kill window, the control chain and the partner go (BT-620 to BT-631)

Rules R4 (healer control, not breaking control) and R6 (the team go) of the PvP rules map, plus the burst gates that go with
them. Decision records: design decision 0032 and the decisions of 2026-11-19 (rules pass, task 1). The class guides that name
these chains are the player guides digested for this module (Frost Mage Deep Freeze into Polymorph or Ring of Frost, Rogue Blind
into a stun chain, Priest Chastise or Fear into Psyfiend, Hunter Scatter Shot into a trap, Resto Druid Cyclone).

## What a bot sees and what the team shares

| Fact | Where it comes from | Shared? |
|------|---------------------|---------|
| The enemy healer's lock (hard control or silence, time left, category, breakable) | the healer's auras, public like a unit frame | read by everyone directly |
| The enemy medallion seen used | the bot's own awareness book (`PvpAwarenessPolicy.h`) | never shared; an expert reads it, a good bot does not |
| Diminishing returns on the healer | the bot's own book, tier filtered (casual bots misread half of them) | never shared |
| The open go (the kill window) and the control offers and pending casts | the match state, one table per side (`SideState::team`) | shared by the side's bots, like the arena plan's focus |
| A partner's burst | the partner's own burst auras (the list in `PvpTeam.cpp`) | public |

Team intel beyond that travels only through visible call-outs (owner, 2026-11-18); nothing here reads another bot's book.

## The kill window (R6)

`PvpTeam::EvaluateWindow`: open when the enemy healer is locked for 4 s or more, or (expert) locked for at least 1.5 s with the
medallion seen used, and the kill target is not immune. Closed when there is no healer (the local burst reasons judge the fight)
and for casual bots (they do not read it). The first bot that sees it open writes the go into the side's table; every member
reads the same go for 12 s (the burst of the guides lasts 15 to 20 s, the opening half is the go).

Where it is used:

- the burst window (`PvpBurstPolicy.h`): reason `team-window` (score 90) and `partner-burst` (score 85), read by good and expert bots;
- the arena focus (`ArenaPlanMath.h`): `burstWindow` raises a hurt target, `healerCcd` raises the other targets while the healer is held.

Partners: a partner's burst aura younger than 10 s on a teammate within 45 yd of the kill target opens the window for the bot.
The bursts of the partners are not made to wait for each other (a bot cannot see a partner's cooldowns); the go is shared the
moment it opens and each member goes as soon as its own burst is ready, which is within one or two ticks. `PvpTeam::Aligned`
(1.5 s) is the measure the live check uses: the first burst aura and the others on the same target.

## The control chain (R4)

The kit of a class is a short table in `PvpTeam.cpp` (spell, diminishing returns category, hard or silence, breakable). Each
bot that is not casual, knows a kit spell and could cast it now (the core's own cast check: range, sight, resource, facing, a
spell that needs a frozen target) scores its options on the enemy healer:

- the length it would land with after the diminishing return (an eighth of a second per point, capped at 8 s), never under 2.5 s;
- plus 20 for a category other than the one running, minus 30 for the same one;
- an instant plus 15 and a cast minus 10 while an enemy with a stopper not seen used is near (expert only);
- a link is due only when the lock left is shorter than the cast time plus a tier slack (expert 250 ms, good 600 ms); a good bot
  casts on a free healer only, so it has no chain; a casual bot casts no team control.

The best offer among the side's bots executes (ties to the lower guid). A cast on its way blocks the same category and a
second hard control on that target for 2.5 s, for every member including the caster, so two bots never waste control into each
other. Healers cast control only while nobody in the group is under 65 percent.

Not in the kit, on purpose: ground and totem controls (Ring of Frost, traps, Solar Beam, Capacitor Totem), which need a
placement, and the controls that strike everyone around the caster (Psychic Scream, Leg Sweep, Howl of Terror), which would
wake the team's own sheep. The off damage dealer as a control target when the enemy has no healer is play v3.

## Not breaking control

A good or expert bot does not cast a damage spell at an enemy player that carries a sheep, a fear, a sleep, a banish, a blind or
a disorient with 0.8 s or more left (`PvpTeamMultiplier`). Melee auto attacks and area damage are not covered; the arena focus
(`ccBreaksOnDamage`) and the burst window (breakable control closes it) steer the team away from a held target already.

## Tiers

| Tier | Window | Control | Hold on breakable control | Burst |
|------|--------|---------|---------------------------|-------|
| Expert | all reasons, medallion read | chain, kick aware, diminishing returns read | yes | waits for a window, falls back after 12 s |
| Good | lock only | first link on a free healer | yes | reads the windows, falls back after 8 s |
| Casual | none | none | no | spends at once |

## Candidate options

Every decision is a candidate with a deterministic integer score (design decision 0014 and 0026): the burst reasons (`PvpBurst::Decide`),
the control options (`PvpTeam::ScoreControl`) and the executor (`ControlBook::IsExecutor`). The log lines `[PVPBURST]` (a change of
answer per bot) and `[PVPTEAM]` (`kind=GO`, `kind=CONTROL`) carry the reason and the score at debug level.

## Open items

- the off damage dealer as a control target when the enemy team has no healer;
- ground controls (traps, Ring of Frost) and the Frost Mage Deep Freeze into Ring of Frost link;
- Psyfiend and Mind Control chains of the priest guides, Alter Time and Barrage rules of the mage guides;
- a readiness peek so that the partners start the go together (their cooldowns are not visible to each other today);
- live tuning of the window thresholds and the tier slacks with the arena bench.
