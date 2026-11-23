# Roadmap: from 0.1.0 to 1.0

A plan, not a promise. Order and scope change with what testing and other servers report. Each step ends with live
checks on a real server, not only a build. Version numbers follow [Semantic Versioning](https://semver.org/); before
1.0, a minor version may change configuration keys (the changelog says how).

## Where 0.1.0 stands

- On by default and in daily use on the maintainer's server: random bots that level, quest, travel and group;
  dungeon finder fill with manners and chat; guilds, breaks, skill tiers.
- On by default, beta: battleground playbooks for six maps, the 2v2 arena ladder, the 1v1 engine, the shared PvP rules,
  bot-led dungeon runs, class and role sheets.
- Off by default, in soak testing: the auction house economy, the social life, the AI sidecar.
- Not run live: the raid strategies.

## 0.1.x: easy to install anywhere

- Build on a bare upstream SkyFire `main` plus `patches/`, add the core pieces that are still missing, and keep that
  build checked in CI together with `tests/run.sh` and the sidecar tests.
- Ship the living world profile (`docs/INSTALL.md` section 5) as an optional configuration file, and review the
  shipped defaults that still follow the base module.
- Tidy the example configuration: remove settings the module no longer reads, fix stale comments.
- Issue templates, a contributing guide and a security note (the sidecar has no authentication).

## 0.2: battlegrounds and duels, proven live

- Live checks for every playbook's open items (each sheet in `docs/bg-sheets/` lists them).
- Playbooks for Temple of Kotmogu and Deepwind Gorge, whose maps come with the core patches.
- The second round of the 1v1 engine, re-measured on the fixed duel matchups in `docs/duels/one-vs-one.md`.
- PvP class packages for the remaining specialisations, each after an in-game test.

## 0.3: the living economy and social life

- Finish the soak of the auction house and the social features on named bots, then decide which can be on by default.
- Trade and city chatter that names goods a bot really listed and recipes it really knows (designed in
  `docs/social/social.md` section 5, not built).
- Bank errands that do real work, and recipe purchases from the crafters' reserve (`docs/economy/reserve-fund.md`).

## 0.4: arenas beyond the pilot

- Human queues against ladder teams.
- 3v3 and 5v5 teams, crowd control chains with diminishing returns, peel, and the map hazards in the cover score
  (`docs/arena/pilot.md`, "Known limits" and "Not in v1").

## 0.5: raids

- First live runs of the raid strategies with bot groups led by a human, starting with the smaller raids, using the
  scenario harness to set up the groups.
- Role sheets extended to raid duties (tank swaps, healer assignments).

## 0.6: scale

- A population design beyond 300 bots: activity scaling, update time and memory limits measured overnight.

## AI layer, along the way

- Duel decisions: the sidecar already has `pvp_engage` and `pvp_reassess` templates; the module does not ask for them
  yet.
- Document how to use another OpenAI-compatible endpoint, if `OPENROUTER_BASE_URL` proves enough for it (untested).

## 1.0

1.0 means: every feature that is on by default has passed its live checks on more than one server, the install works
from a bare upstream SkyFire with the patch set (or with hooks merged upstream), the configuration keys are stable, and
the documents match the code. As the generic core hooks find their way into SkyFire upstream, the patch set shrinks.

Ideas, feedback and help are welcome in the issues, especially from servers with other populations and other hardware.
