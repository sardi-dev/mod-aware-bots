# Features

Everything the module does in v0.1.0, grouped by area. Each line names the document or the configuration key
behind it (all keys are in `conf/playerbots.conf.dist`, each with a comment), so you can check the claim and switch the
feature off.

**State** says how far a feature has got:

- **in use**: on by default and part of the daily running of the maintainer's server during the private weeks.
- **beta**: on by default or ready to switch on, implemented and unit tested; live runs found and fixed problems, and
  some recent fixes still wait for their live check. Each design sheet says at the top what is verified.
- **off, beta**: off by default, in soak testing on a few named bots.
- **not run live**: implemented, never run on a live server.

The module builds on mod-playerbots for AzerothCore and the Legends of Azeroth 5.4.8 version of it. Features marked
"inherited" come from that base, adapted to SkyFire; the rest were written or reworked for this module.

## World and leveling

| Feature | What it does | Source | State |
|---------|--------------|--------|-------|
| Random bots | Bot accounts and characters that log in on their own and live in the world | `AiPlayerbot.MinRandomBots`, `MaxRandomBots` (default 500; the largest population tested is about 300), `RandomBotAccountCount` | in use (inherited, reworked) |
| Random passwords | Bot accounts get random passwords | `AiPlayerbot.RandomBotRandomPassword` (1) | in use |
| Higher batch | A second set of bot accounts whose characters start once at a weighted level band with uncommon gear, then earn everything | `AiPlayerbot.HigherBatch*` (off: 0 accounts) | beta |
| Roaming with purpose | Bots draw an activity (quest, grind, gather, visit an NPC, wander) from weights, with questing kept first while the zone has quest work | `AiPlayerbot.EnableNewRpgStrategy`, `AiPlayerbot.RpgStatusProbWeight.*` | in use (inherited, reworked) |
| City errands | Bots in a city see to their needs first (repair, vendor, trainer, mail), otherwise pick an errand (bank, auction house, inn, mailbox) with a dwell time and gestures; with the economy on, the auction errand really trades | `docs/social/social.md` sections 1 and 6 (`ErrandPolicy`) | beta |
| Travel | Bots walk, ride and take flight paths; a travel graph routes them across continents by boats and zeppelins | `AiPlayerbot.EnableTravelNodes` (0; the graph is built once into the playerbots database, which takes minutes to hours and blocks that start) | beta |
| Stranded recovery | A bot stuck in a closed pocket of the navigation mesh walks out, then uses its Hearthstone | `AiPlayerbot.StrandedRecovery.*` | beta |
| Quest give-ups | A bot remembers why a quest failed (stuck on the way, stood still, a stalled walk to a boat or portal) across picking it up again and backs off a quest that keeps failing | `src/Utils/QuestRetryPolicy.h` | in use |
| Breaks and log-offs | Solo bots take breaks at inns or quiet spots, group bots rarely and announced; some bots log off for a while and come back | `AiPlayerbot.BotBreaks.*` | in use |
| Activity near humans | Bots in a human's zone or within a radius act fully; elsewhere a share acts at a time, thinned further when the server lags | `AiPlayerbot.BotActiveAlone`, `BotActiveAloneForceWhenInZone`, `BotActiveAloneForceWhenInRadius`, `botActiveAloneSmartScale` | in use (inherited) |
| Training | Alt bots walk to a class trainer and pay for their spells; random bots learn class spells on level-up by default; riding and professions are bought at trainers with the bot's own gold | `AiPlayerbot.AltBotsVisitTrainers`, `AiPlayerbot.AutoLearnTrainerSpells` (1), `docs/economy/reserve-fund.md` section 3 | beta |
| Gear | Bots equip upgrades they loot; alt bots also buy upgrades at vendors and craft them with their own professions | `AiPlayerbot.AutoEquipUpgradeLoot`, `AutoUpgradeEquip`, `AltBotsShopForGear`, `AltBotsCraftGear` | beta |
| No cheats, no free upkeep | No food, taxi or raid cheats (removed from the code); no free repair or revive on death; no maintenance, autogear or best-in-slot commands | `AiPlayerbot.BotCheats` (retired), `docs/raids.md` ("Removed cheats"), `docs/decisions.md` (0016, 0017) | in use |
| Skill tiers | Each random bot has a fixed tier from its guid: experts play their whole class sheet and react at once, good bots leave out the expert lines and react a little late, casual bots play the core lines and are slow | `AiPlayerbot.SkillTiers.*` (10 percent expert, 60 good) | in use |
| Bot guilds | Bots found, fill and run guilds with each other: invites, promotions, welcomes, members who drift away; an optional offer to guildless humans | `AiPlayerbot.BotGuilds.*` (`InviteHumans` 0) | in use |
| Summons | Bots accept a group mate's summon after a pause with a thank-you, decline a stranger's, and use meeting stones to summon a far group mate | `AiPlayerbot.AcceptSummons.*`, `AiPlayerbot.StoneSummons.*` | beta |
| Duel spots | Bots gather at duel spots in towns (Goldshire, Razor Hill, the Pandaria shrines and more) and duel each other | `AiPlayerbot.DuelSpotEnable`, `AiPlayerbot.DuelSpot.*` | beta |

## Groups and dungeons

| Feature | What it does | Source | State |
|---------|--------------|--------|-------|
| Goal-based invites | A random bot weighs your invite against its own state (level gap, a quest objective close by, a task it just started, friends and guild) and may decline with a reason | `AiPlayerbot.GoalInvites.*` | in use |
| Travel to the group | A random bot in your group that is far away travels to you and reports on the way, instead of being summoned | `AiPlayerbot.TravelToGroup.*` | in use |
| Alt bots | Your own characters as group members; a bot account can lend its characters to a main account | `AiPlayerbot.AllowBotAccounts`, `.playerbots account botmode`, `.playerbots bot add` | in use (inherited, reworked) |
| Leading by marks | The bot tank pulls the skull when the group is ready (alive, near, health and mana up); damage dealers follow the marks | `AiPlayerbot.TankPullsMarks`, `MarkPullRange`, `docs/role-sheets/tank.md` | beta |
| Loot manners | Bots congratulate a rare win, ask before needing an item you could use, and offer you an item they won but cannot use | `AiPlayerbot.LootEtiquette.*`, `AiPlayerbot.LootOwnerFirst` | beta |
| Dungeon finder fill | When a human queues, the module counts the roles already queued and adds fitting bots (faction, level window) for the missing ones; a damage bot of a class that can tank switches spec when no tank fits; a filled tank is at most one level under the player (`AiPlayerbot.LfgFill.TankLevelBelowPlayer`), and a bot that cannot get into the dungeon leaves after three teleport asks | `AiPlayerbot.LfgFill.*` | in use |
| Role balance | A random bot picking its first specialization prefers tank or healer when free bots of its faction and level band lack that role (25 / 20 / 55, at least four free tanks and healers per band from 25 to 54); existing specs never change | `AiPlayerbot.SpecRoleBalance.*` | beta |
| Finder manners | Finder bots greet, ask whether anyone needs quests, say they are ready, thank after the last boss, wait for the loot and leave with a goodbye; they never leave mid-run | `AiPlayerbot.LfgManners.*` | in use |
| Dungeon chat | Small talk at the moments of a run (start, boss pull and kill, a death, a long fight, low mana, a quick drink), short answers to simple phrases (`ready?`, `brb`, `thanks`), all rate limited | `AiPlayerbot.DungeonChat.*` | in use |
| Bot voice | A stable personality per bot (quiet, friendly, joker, grumpy) picks its line variants and how much it talks | `AiPlayerbot.Voice.*` | in use |
| Bot-led dungeon runs | Now and then five free bots queue together and run a dungeon on their own: the tank leads boss to boss, waits for the group, handles the event steps of several dungeons (escorts, doors, the Deadmines cannon, the Culling of Stratholme); a run whose bosses all stay out of reach ends early and the dungeon rests for a few hours (`AiPlayerbot.LfgBotRuns.FailureCooldownHours`) | `AiPlayerbot.LfgBotRuns.*`, `AiPlayerbot.LfgCrawl.*` | beta |
| Bots form groups by hand | A bot posts "LFM" in the LookingForGroup channel, gathers bots (and humans who whisper `inv`), meets at the entrance and walks in | `AiPlayerbot.BotLfm.*` | beta |
| Leftover bot groups | A bot-only group whose leader has been away (offline, another map, far off) for minutes breaks up, and a tank drops a mark it cannot reach | `AiPlayerbot.StaleGroups.*` | beta |
| Ledge recovery | A bot stranded on a ledge above its leader searches up to 30 yd for a way down to the leader's floor and walks off in view, never vanishing | `src/Utils/LedgeWalkOffPolicy.h` | beta |
| Water | Bots swim at the surface and dive to a target deep under it | `AiPlayerbot.SurfaceSwimAllMaps` | beta |
| Dungeon strategies | Boss mechanics for 47 dungeons from the original game through the 5.4.8 dungeons | `src/strategy/dungeons/` | beta (mostly inherited) |

## Raids

| Feature | What it does | Source | State |
|---------|--------------|--------|-------|
| Raid strategies | Strategies for 20 raids, from Molten Core to the Ruby Sanctum, active only on their map; every spell id checked in the 5.4.8 data | `docs/raids.md` | not run live (mostly inherited) |
| Cheat audit | Every teleport, aura removal, forced kill and speed boost of the ported raid code was removed or turned into a walk | `docs/raids.md` ("Removed cheats" columns) | not run live |

## Classes and roles

| Feature | What it does | Source | State |
|---------|--------------|--------|-------|
| Rotations | A rotation for each of the 34 specialisations | `src/strategy/Classes/` | in use (inherited, reworked) |
| Class sheets | Talents, glyphs, stat order and rotation priority per specialisation, compared across player guides (cited by name and date), checked against the 5.4.8 spell data and against what the bots did; 28 sheets are in the code, 6 are drafts | `docs/class-sheets/`, `docs/class-sheets/damage-status.md` | beta |
| Talents and glyphs | Premade talent and glyph sets per specialisation from the class sheets | `AiPlayerbot.PremadeSpecLink.*`, `AiPlayerbot.PremadeSpecGlyph.*` | beta |
| Tank role | Owning the fight: marks only, waiting for the healer's mana, taking every mob that hits a member | `docs/role-sheets/tank.md` | beta |
| Healer role | Shields and heals over time on the tank before and during pulls with mana guardrails, triage, drinking between pulls | `docs/role-sheets/healer.md`, `AiPlayerbot.DungeonChat.HealerDrinkPercent` | beta |
| Damage role | Target choice by marks, threat discipline, interrupts and crowd control duty, cooldowns by fight | `docs/role-sheets/dps.md` | beta |
| Dispel timing | No dispel on an aura that is about to run out | `AiPlayerbot.DispelAuraDuration` | beta |

## PvP

### Battlegrounds

| Feature | What it does | Source | State |
|---------|--------------|--------|-------|
| Queue fill | Bots fill the battleground queues humans wait in; optionally they open queues themselves | `AiPlayerbot.RandomBotJoinBG` (1), `AiPlayerbot.RandomBotAutoJoinBG` (0), `AiPlayerbot.RandomBotAutoJoin*` | beta (inherited) |
| Playbooks per map | A written plan per map (goals, positions, roles, opening, mid-game rules, call-outs, failure modes, tests), then built in code: Warsong Gulch, Twin Peaks, Arathi Basin, Battle for Gilneas, Eye of the Storm, Silvershard Mines, Temple of Kotmogu (partial: score-based leader stances and expiring carrier calls) | `docs/bg-sheets/` | beta |
| Other maps | Alterac Valley, Strand of the Ancients, Isle of Conquest and Deepwind Gorge use the tactics of the base module or the shared objective layer | `src/strategy/bg/` | beta (inherited) |
| Leader bot | One bot per side, never a human, confirms the stance (balanced, all out, defense and so on) at the gates and at every change | `AiPlayerbot.Pvp.Leader.Enable` | beta |
| Call-outs | Bots call the enemy flag carrier, flags taken and dropped, enemies at a base, the score near the end; expert bots call every event, casual bots fewer and later | `AiPlayerbot.BattlegroundChat.*` | beta |
| Human call-out reader | A short line such as `inc bs 3`, `efc mid` or `help fr` is read as information, never as a command; it ages, never overrides what the plan sees, and the nearest bot answers "omw" | `AiPlayerbot.Pvp.CalloutReader.*` | beta |
| Seen-only intel | On the node maps (Arathi Basin, Battle for Gilneas, Eye of the Storm) bots count only the enemies a living teammate can see or a call-out placed, never the whole roster | `src/Utils/AbPlanLogic.h` | beta |
| New maps in the core | Silvershard Mines, Deepwind Gorge, the Tiger's Peak arena and the reworked Temple of Kotmogu rules | `patches/` 0017 to 0022 | beta |

### Arenas

| Feature | What it does | Source | State |
|---------|--------------|--------|-------|
| Bot arena ladder | Named 2v2 teams of random bots, chosen by a composition score, with their own rating (the core keeps none), a schedule of bot-versus-bot matches that avoids a busy server, retirement and partner replacement | `AiPlayerbot.ArenaTeams.*`, `docs/arena/pilot.md` | beta (pilot) |
| Ladder NPC | The "Pit Chronicler" beside the arena battlemasters shows the ladder; it is spawned at run time and never saved | `AiPlayerbot.ArenaTeams.LadderNpc`, `data/sql/db-world/` | beta |
| Ladder news | New teams, streaks and rivalries in the General channel, rate limited | `AiPlayerbot.ArenaChat.*` | beta |
| JSON snapshot | The ladder as a JSON file for a status page | `AiPlayerbot.ArenaTeams.SnapshotFile` | beta |
| Team play | A shared focus, defensives and cover from casters out of their line of sight | `docs/arena/pilot.md` ("Play v1") | beta |

### Duels and shared PvP rules

| Feature | What it does | Source | State |
|---------|--------------|--------|-------|
| 1v1 engine | In duels, open-world fights and isolated battleground skirmishes ranged bots slow, step back and use their escapes; melee bots close gaps and slow fleeing enemies; reaction times by tier (expert 250 to 350 ms, casual 600 to 900 ms) | `AiPlayerbot.OneVsOne.Enable`, `docs/duels/one-vs-one.md` | beta |
| Awareness | Each bot keeps a book of what it saw the enemy use (trinket, defensives, interrupts, diminishing returns), filtered by tier; books are not shared | `AiPlayerbot.Pvp.Awareness.Enable`, `docs/pvp/awareness-r1-r2.md` | beta |
| Interrupts | Kick what matters, late in the cast, one kicker per cast | `AiPlayerbot.Pvp.Interrupts.Enable` | beta |
| Defensives | The smaller defensive early, one per enemy burst, the cheapest way out of control before the medallion | `AiPlayerbot.Pvp.Defensives.Enable` | beta |
| Fake casts | Expert casters cancel an important cast while an unseen interrupt is ready | `AiPlayerbot.Pvp.FakeCast.*` | beta |
| Burst window | Big cooldowns are spent in a kill window against players | `AiPlayerbot.Pvp.Bursts.Enable` | beta |
| Team play | The enemy healer's lock opens a shared kill window; crowd control on the healer has one executor and respects diminishing returns | `AiPlayerbot.Pvp.TeamPlay.Enable`, `docs/arena/team-play.md` | beta |
| Class packages | PvP play for Frost Mage, Mistweaver, Affliction, Restoration Druid, Arms, Elemental, Holy Paladin, Shadow Priest and Subtlety, active only against players | `docs/pvp/class-packages.md` | beta (each waits for an in-game test) |

## Economy

| Feature | What it does | Source | State |
|---------|--------------|--------|-------|
| Auction house selling | Random bots list whole stacks of what they looted, gathered or crafted at their faction's auction house, with the vendor trip's protections; listings run 12 or 24 hours | `AiPlayerbot.Auction.Enable` (0), `AiPlayerbot.Auction.LongListingMinGold`, `docs/economy/auction-wiring.md` | off, beta |
| Auction house buying | Bots buy what they need with buyouts (never bids), from a quarter of their gold above the reserve per visit | `AiPlayerbot.Auction.Buy.Enable`, `MaxPurchasesPerVisit` | off, beta |
| Mail | Bots collect the gold of sales and their expired items from the mailbox | `docs/economy/auction-wiring.md` | off, beta |
| Price table | One price range per listable item from the 5.4.8 data: never below what a vendor pays, gold-vendor items at least 10 percent above the vendor price, profession goods priced from their reagents, dungeon and raid gear above world drops | `docs/economy/pricing-audit.md`, `data/sql/db-world/` (`playerbots_item_price`) | off, beta |
| Reserve fund | Before buying, a bot keeps gold for repairs, its next trainer spells, its next riding skill and a mount; crafters keep a share for recipes | `AiPlayerbot.Auction.ReserveLookaheadLevels`, `ReserveGoalSharePercent`, `ReserveRecipeSharePercent`, `docs/economy/reserve-fund.md` | off, beta |
| Recipe purchases | Random crafters buy and use eligible vendor recipes on shopping trips within their recipe reserve and limits | `AiPlayerbot.RecipeBuying.*`, `AiPlayerbot.Auction.OnlyBots` | off, beta |
| Bank errands | Random bots deposit retained whole stacks during banker visits, within a per-visit limit | `AiPlayerbot.BankErrands.Enable`, `AiPlayerbot.BankErrands.MaxMoves`, `AiPlayerbot.Auction.OnlyBots` | off, beta |
| Profession materials | The auction keeps the materials a bot uses itself, in three levels by use | `docs/economy/auction-wiring.md` | off, beta |
| Trial scope and limits | Limit the auction house to named bots, cap listings and visits, spread the work over world ticks, log a summary | `AiPlayerbot.Auction.OnlyBots`, `MaxActiveListings`, `VisitCooldownMinutes`, `SlicesPerTick`, `SummaryMinutes` | off, beta |
| Soak measurements | The auction summary reports work per world tick (average and peak ms), waiting visits, timeouts, snapshot truncation and gold reconciliation (deposits, sale gold mailed and collected); vendor gear purchases log one line each | `[AUCTION] summary`, `[BUYGEAR]` | beta |
| Economy tools | Scripts that rebuild the price and reserve tables from the data and measure the effect on the bots' gold | `tools/economy/` | beta |

## Social life

| Feature | What it does | Source | State |
|---------|--------------|--------|-------|
| Liking score | A score from 0 to 100 per bot and human: grouping, a finished dungeon, a completed trade (money or items really moved), a duel, guild time and thanks raise it; insults, kicks and ninja looting lower it; it drifts back to neutral; daily gain is capped | `AiPlayerbot.Social.Liking.*` (0), `docs/social/social.md` section 2 | off, beta |
| Liking at work | The score changes the chance a bot accepts your invite and gates group and guild offers | `AiPlayerbot.Social.Liking.InviteSpan`, `QuestOfferMin`, `GuildOfferMin` | off, beta |
| Playstyles | Each random bot gets a lasting style (quester, gatherer-crafter, trader, socializer, PvPer) that leans its activity weights its own way | `AiPlayerbot.Social.Playstyle.*` (0) | off, beta |
| Shared-quest offers | A bot with the same unfinished quest as a human nearby, who likes that human, whispers an offer and sends an ordinary group invite; capped per human per hour | `AiPlayerbot.Social.QuestOffer.*` (0) | off, beta |
| Trade chatter | Traders advertise their active listings and crafters offer known recipes in capital Trade channels when humans are present | `AiPlayerbot.Social.TradeChatter.*`, `AiPlayerbot.Social.OnlyBots` | off, beta |
| Emote replies | A bot you `/wave` or `/dance` at answers with a fitting gesture, at most once a minute per human | `AiPlayerbot.Social.EmoteReplies.*` (0) | off, beta |
| Privacy | The liking table holds two guids, a number and timestamps, no names; rows expire; deleting a character removes its rows; `.playerbots social wipe <name>` (or `all`) deletes a player's rows (and the sidecar's memory of them) | `AiPlayerbot.Social.Liking.RetentionDays`, `docs/social/social.md` section 7 | off, beta |
| Trial scope | Limit the social features to named bots, with per-action log lines and a periodic summary | `AiPlayerbot.Social.OnlyBots`, `SoakLog`, `SummarySeconds` | off, beta |

## AI layer (optional sidecar)

| Feature | What it does | Source | State |
|---------|--------------|--------|-------|
| Sidecar | A small Python service (FastAPI) that the world server talks to over HTTP on a private network; it reaches a language model through OpenRouter with a key from the environment | `services/aware-ai/README.md`, `AwareAi.Enable` (0), `AwareAi.SidecarUrl` | off, beta |
| Player-to-player chat | Bots answer humans (never other bots) when challenged to a duel, after a duel, on a whisper, in say, party, raid and guild chat, at group moments and at the end of a battleground; trivial lines get the module's own template lines without a model call | `AwareAi.Chat.Enable` (0), `AwareAi.Chat.*` | off, beta |
| Human timing | Replies wait a reading and typing time, some come late like a busy player, combat delays or drops some, and a conversation keeps going while it is fresh | `AwareAi.Chat.DeadlineMs`, `LateChance`, `CombatWaitChance`, `CombatSkipChance` | off, beta |
| Openers and friendly whispers | Rare first lines: a hello when you join a bot group, a "gz" on your level-up, a duel ask, a whisper from a bot you grouped with | `AwareAi.Chat.Opener.*`, `AwareAi.Chat.FriendWhisper.*` | off, beta |
| Battleground strategy | The leader bot asks which stance, floater job, wipe reply or target node to play, only when two materially different legal options exist; the rules play at once and keep playing when the answer is late, stale or unsure | `AwareAi.Pvp.Enable`, `AwareAi.Pvp.BgIntervalSeconds`, `MinConfidence`, `MaxRequestsPerMatchSide` | off, beta |
| Local fallback | Every failure (sidecar down, timeout, budget or rate limit, empty answer) falls back to the bot's rule line or rule plan; a fake mode answers without a key and without cost | `AwareAi.DecisionDeadlineMs`, `AWARE_AI_FAKE` | off, beta |
| Budget | One monthly pool in US dollars shared by chat and decisions, enforced before each call, with an optional daily cap and a daily alarm; a circuit breaker after repeated failures | `AwareAi.Budget.MonthlyUsd` (20), `AwareAi.Budget.DailyUsd`, `AWARE_AI_MONTHLY_CEILING_USD`, `AWARE_AI_BREAKER_*` | off, beta |
| Saying it is a bot | When a player asks, a bot says plainly that it is a bot; a switch makes it deny it playfully instead | `AwareAi.Chat.DenyBeingBot` (0), `services/aware-ai/README.md` ("Admitting to being a bot") | off, beta |
| Memory | The sidecar remembers what a bot and a human shared (grouped, a boss together, a duel, loot passed, thanks) and can carry the module's liking band in its prompt | `AwareAi.Chat.SendLiking` (0), `services/aware-ai/README.md` ("Liking score and wipe") | off, beta |
| Privacy | Requests carry game context only; emails, IP addresses and obvious sensitive fields are stripped before provider calls and logs; chat requires zero-data-retention routing by default; logs expire after 90 days; a wipe endpoint forgets a player | `AWARE_AI_CHAT_ZDR` (1), `AWARE_AI_LOG_RETENTION_DAYS`, `POST /v1/memory/wipe` | off, beta |
| Commands | `.bot ai status`, `on`, `off`, `budget` for game masters and administrators | `data/sql/db-auth/` (permissions 1347, 1348) | off, beta |

When chat is on, the human's chat line and the character names in the conversation go to OpenRouter and the model
provider it routes to. The sidecar also ships decision templates for duels (`pvp_engage`, `pvp_reassess`); the module
does not ask for them in this release.

## Testing and quality

| Feature | What it does | Source | State |
|---------|--------------|--------|-------|
| Unit tests | More than 1,700 tests of the module's pure decision logic (battleground plans, arena math, duels, pricing, reserve fund, liking, playstyles, chat pacing, pathing helpers), built with only `g++` and no framework | `tests/run.sh` | in use |
| Sidecar tests | About 100 tests with OpenRouter mocked and no network, plus lint, format and strict type checks | `services/aware-ai/tests/`, `pyproject.toml` | in use |
| Sidecar round trip | A C++ harness that checks the module's request and answer bodies against the sidecar | `tests/aware_ai/` | in use |
| Scenario harness | Game master commands that seed and drive reproducible situations (battlegrounds with chosen bots and strategies, raid groups around a probe, duels) and trace what the bots decide | `.botscenario`, `.botprobe` (permission 1345) | in use |
| Live measurement | Fixed duel matchups, bot-only battleground matches and dungeon runs, measured with a headless test client (not part of this release) | `docs/duels/one-vs-one.md`, `docs/bg-sheets/` | in use |
| Performance monitor | Per-action timing for finding slow strategies | `AiPlayerbot.PerfMonEnabled` (0), `.pmon` | inherited |
| Log tags | Tagged log lines per feature (`[BOTPARTY]`, `[ARENALADDER]`, `[AUCTION]`, `[SOCIAL]`, `[UNSTUCK]` and more) | the comments in `conf/playerbots.conf.dist` | in use |
| Stub list | Every place where SkyFire lacks something the module uses, generated from the code | `docs/stubs.md`, `tools/port/stubs.py` | in use |
| Uninstall | SQL that removes the module's world data again | `data/sql/uninstall/` | in use |
