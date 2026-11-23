# Design decisions

Comments, configuration and sheets in this repository cite numbered design decisions ("design decision 0032 point 9").
They come from the maintainer's decision log, which is not published. The numbers that are cited, and what each one
decided, in one line:

| No. | Subject |
|-----|---------|
| 0008 | A compatibility layer that gives ported code the TrinityCore names on SkyFire (`src/compat/`) |
| 0010 | Facts about MoP mechanics are cross-checked against public MoP Classic data |
| 0011 | Build the module from Legends of Azeroth's 5.4.8 version of mod-playerbots |
| 0013 | How the port is built: separate `playerbots` database, small additive core hooks, original headers kept |
| 0014 | An optional AI layer decides PvP duels and fights; PvE stays deterministic |
| 0015 | PvP intelligence tiers: casual, good and expert bots play at different skill |
| 0016 | Autonomous bots with human controls: no free maintenance, no free gear, no hand-outs |
| 0017 | Bots earn what they use (supplies, skills, mounts) the way a player does |
| 0018 | Bot accounts and how they are named and owned |
| 0020 | Alt bots (the player's own characters) pick spec, talents and gear on their own |
| 0021 | Bots equip upgrades they own, buy and craft like a player |
| 0025 | A second, higher batch of random bot accounts that start once |
| 0026 | The AI sidecar uses its own provider key |
| 0027 | Bot arena teams: named teams, their own rating, a ladder NPC |
| 0028 | Modules are optional and plug and play; the core's database updater applies module SQL |
| 0029 | The AI layer's player-to-player chat and its models |
| 0031 | AI chat behaviour and one shared monthly budget for the sidecar |
| 0032 | The PvP, arena and battleground system: tiers, a leader bot, playbooks per map |
| 0033 | A living economy: the bots' auction house, the reserve fund, social life of the bots |
| 0034 | The price model of the bots' auction house |

The decisions that shaped the code are restated where they matter: the module SQL layout in `README.md` and
`patches/README.md`, the economy in `docs/economy/`, the social design in `docs/social/social.md`, the PvP system in
`docs/pvp/`, `docs/arena/` and `docs/bg-sheets/`.
