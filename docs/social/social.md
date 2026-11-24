# Social life of the random bots

Design sheet for design decision 0033 points 13 to 17 (owner answers of 2026-11-22). It says what the module does today (file and
function), the design of each feature, the privacy rules, and the ordered list of gaps. Everything here sits behind an
`AiPlayerbot.*` switch that is off until the unit tests and an overnight soak pass (design decision 0033 point 20). Slice 1 is in the tree:
the two decision policies (`src/Utils/LikingPolicy.h`, `src/Utils/PlaystylePolicy.h`), their tests, the table and the config
keys. Slice 2 (section 11) wires them in; slice 3 (section 12) adds the sidecar fields and wipe, emote replies and shared-quest
group offers; everything stays off until the soak.

Owner rules (2026-11-22): the liking score lives in the local playerbots database and also goes into the AI sidecar's prompts
when the sidecar is active; each bot has a lasting playstyle that multiplies the global weights; group offers and guild invites
are gated by liking and capped per hour per human; cities get checked hubs, errands that do real work and answers to human emotes,
with no teleported idlers and no world events; bots move with purpose.

## 1. What exists today

| Area | Where | What it does now | Gap for this sheet |
|------|-------|------------------|--------------------|
| Group invites from a human | `src/strategy/actions/AcceptInvitationAction.cpp`, `DecideInvite` (settings `GoalInviteConfig`, keys `AiPlayerbot.GoalInvites.*`) | A random bot rolls: base 70, a stable per-bot lean of +-10 (`guid % 21`), minus level, quest-nearby and new-task penalties, plus `FriendBonus` 15 (friend list, either way), `GuildBonus` 20 (same guild) and a same-zone bonus 10, clamped to 5..95; a decline whispers its reason | Ties are flat and binary. A human the bot has played with for weeks is treated like a stranger unless on the friend list |
| Guilds | `src/Manager/BotGuilds.cpp`: `Update`, `FoundGuild`, `Recruit`, `ProcessPending`, `ManageGuilds`, `OfferToHumans` (keys `AiPlayerbot.BotGuilds.*`) | One world-thread pass per 60 s: bots found, fill and run guilds with each other. `OfferToHumans` (off, `InviteHumans = 0`): a recruiting leader near a guildless human whispers and sends the ordinary invite, 5 percent per pass, 24 h cooldown per human, only the leader | The offer ignores history: any human near a leader is a target, no bot "knows" them. Cooldown is per human in memory, lost at restart |
| Activity weights | `PlayerbotAIConfig::Initialize` (`RpgStatusProbWeight[]`, keys `AiPlayerbot.RpgStatusProbWeight.*`), drawn in `NewRpgBaseAction::RandomChangeStatus` (`NewRpgBaseAction.cpp`) | One global table for every bot: the draw sums the weights of the available statuses, then `QuestLevelingPolicy::KeepInDraw` trims grind and flight when the zone has quest work | All bots draw from the same table, so they look alike |
| City errands | `src/Utils/ErrandPolicy.h` (`Choose`, `DwellMs`, `AnotherErrand`), `src/strategy/RPG/NewRpgAction.cpp` (`FindErrandTargets`, the wander-NPC dwell state machine) | BT-253: needs first (repair, vendor, trainer, mail), else idle errands by weight (bank 30, auction 30, inn 25, mailbox 15), a dwell time per errand, gestures, a second errand 60/30/0 percent | Weights are global, no hub list, no human-aware behaviour |
| Emotes | `src/strategy/actions/EmoteAction.cpp` (`ReceiveEmote`, 5.4.8 packet parse), triggers in `src/strategy/generic/EmoteStrategy.cpp` | The AC reaction table exists but only runs with `AiPlayerbot.RandomBotEmote = 1` (default 0), and several cases act for the master only | Not on for random bots; no limit per human |
| Trade, General and LFG lines | `src/Utils/BroadcastHelper.cpp` (`BroadcastToChannelWithGlobalChance`, `BroadcastSuggestSell`, `...Instance`, `...Quest`, `...Something`), driven by `src/strategy/actions/SuggestWhatToDoAction.cpp` (`isUseful`: a solo random bot, 30 s after its last line; `Execute` picks one of six suggestions at random) | AC's broadcast chances (`AiPlayerbot.Broadcast*`). A sell line names an item from the bot's bags and the vendor price | Lines are not tied to real listings or real recipes, not to who is in the city, and have no per-hour cap per faction |
| AI chat | `src/AwareAi/AiChat.cpp` (`OnWhisper`, `OnGroupMemberAdded`, `OnDuelEnd`, `OnDungeonDone`, `OnLootRollWon`, `OnHumanUpdate`, `SendMemory`, `NoteTone`), `src/AwareAi/AwareAiProtocol.h` (`AwareAiChatRequest`, `AwareAiMemoryEvent`) | Sends player-to-player moments to the sidecar (design decision 0029, 0031). Moments between a bot and a human go to `POST /v1/memory/event` (grouped, dungeon_done, boss_killed_together, wipe_together, duel_won, duel_lost, loot_ninja, loot_passed, insulted, thanked). The friendly whisper uses an in-memory companion list (`NoteCompanion`, lost at restart) and the sidecar's own score | The sidecar keeps its own pair score (`services/aware-ai`, `storage.py` `RELATION_WEIGHTS`, -100..100, `FRIENDLY_SCORE` 20) that the module never sees and cannot wipe |
| Persistence | `sql/playerbots/updates/db_playerbots/`, `src/Arena/ArenaStore.cpp` | The module has no SQL updater of its own: the arena store creates its tables in code (`CREATE TABLE IF NOT EXISTS`) and the update file is for fresh installs | The liking table needs the same treatment (section 7) |

## 2. Liking score (point 14)

### 2.1 Data

- Table `playerbots_social_liking` (`sql/playerbots/updates/db_playerbots/2026_11_22_00.sql`): `bot_guid`, `player_guid`
  (characters.guid, low part), `value` (FLOAT in [0, 1]), `updated_at` (unix seconds), `gain_day`, `gain_today` (the daily cap
  state). Primary key on both guids, index on `player_guid`. No names are stored.
- A pair with no row is neutral (0.30). Only random bots and humans have rows; alt bots (a player's own characters) have none.
- Runtime: a map from the pair to `LikingPolicy::Pair`, loaded for a human at login (one query on `player_guid`), written back
  by a world-thread flush every 60 s for dirty rows (one multi-row `REPLACE`, async), and at logout. Nothing is read from the
  database on a bot's tick.

### 2.2 Events and deltas (`LikingPolicy::Tuning`, config `AiPlayerbot.Social.Liking.Delta.*`, in points of 100)

| Event | Delta per step | Source in the module | Notes |
|-------|----------------|----------------------|-------|
| Group time | +1 per 10 full minutes together | a new world-thread pass (every 60 s) over humans in a group with bots; minutes accumulate in memory and are applied in whole steps | Counts bot-and-human pairs of the same group only, never in a battleground queue wait |
| Dungeon done | +6 | `AiChat::OnDungeonDone` call site `LfgMannersAction.cpp:283` | Finder or party run finished with the human |
| Traded | +2 | `TradeStatusAction.cpp` at the `TRADE_STATUS_ACCEPTED` branch of `Execute`, when an item or gold moved | A bot gift to the human counts the same; the daily cap stops farming |
| Dueled | +2 | `PlayerbotsPlayerScript::OnDuelEnd` (`mod_playerbots.cpp:447`), for a duel decided or fled | Win or loss does not matter; an interrupted duel counts nothing |
| Guild time | +1 per full hour online in the same guild | `BotGuilds::Update` pass (it already walks the guilds every 60 s) | Per bot member of the guild; the cap applies per pair |
| Thanked | +2 | `AiChat::NoteTone` (`ToneOf` in `AiChatLines.h`) | Must also work with the AI layer off, so the tone check moves to a hook before the `ChatOn()` test |
| Insulted | -10 | `AiChat::NoteTone` | Not capped |
| Kicked | -8 | `PlayerbotsGroupScript::OnRemoveMember` (`mod_playerbots.cpp:498`) with a human kicker and a bot member (new override) | Not capped; ignore a kick inside the first 2 minutes of the group (a mis-click) |
| Loot ninja | -15 | `AiChat::OnLootRollWon` | Matches the sidecar's `loot_ninja` event |

A new pair is `Fresh` (neutral). `Apply` first decays the stored value to now, then adds the change; a positive change is cut
to what is left of the day's cap, a negative one is not.

Why these numbers: a first evening in a four-bot group (two hours and one dungeon) would add 12 + 6 = 18 points but the daily cap of
15 holds it at 45, which unlocks quest offers (40). A second evening reaches 60, a friend (55). A friend left alone loses half of
the distance to neutral every 21 days: 55 becomes 42 after three weeks and 36 after six, so a friendship has to be kept up. A
single insult (-10) takes a stranger from 30 to 20, the edge of cold; a second one makes the bot cold.

### 2.3 Decay, caps, thresholds

- Decay: `value = neutral + (value - neutral) * 2^(-elapsed / halfLife)`, half-life 21 days (`DecayHalfLifeDays`), from either
  side, so a grudge fades too. Worked out when a row is read or changed, never by a timer.
- Daily cap: positive moves of one pair add up to 15 points per UTC day (`DailyGainCap`), stored in the row.
- Bands (`BandOf`, the word the sidecar prompt carries): cold below 20, stranger up to 55, friendly up to 75, close from 75.
- Gates: a shared-quest group offer needs 40 (`QuestOfferMin`), a guild offer 55 (`GuildOfferMin`). Offers per human: 2 group
  offers and 1 guild offer per hour from all bots together (`QuestOffersPerHour`, `GuildOffersPerHour`, `OfferWindow`; the
  window is kept in memory per human and kind).
- Invite chance: `InviteChanceDelta = round((value - neutral) * InviteSpan)`, span 60: a friend adds +15 (the size of the flat
  `FriendBonus`), a close friend +27, a cold human -12 or worse, a human at 0 -18. The existing `FriendBonus` and `GuildBonus`
  stay (explicit ties of the player); the final clamp 5..95 still applies.

### 2.4 Config (all in `conf/playerbots.conf.dist`, `PlayerbotAIConfig::socialLiking`)

`AiPlayerbot.Social.Liking.Enable` (0), `Neutral` (30), `DecayHalfLifeDays` (21), `DailyGainCap` (15), `GroupStepMinutes` (10),
`Delta.GroupStep|DungeonDone|Traded|Dueled|GuildHour|Thanked|Insulted|Kicked|LootNinja` (1, 6, 2, 2, 1, 2, -10, -8, -15),
`ColdBelow` (20), `FriendlyAt` (55), `CloseAt` (75), `InviteSpan` (60), `QuestOfferMin` (40), `GuildOfferMin` (55),
`QuestOffersPerHour` (2), `GuildOffersPerHour` (1), `KickGraceSeconds` (120, slice 2), `RetentionDays` (180).

### 2.5 How the score reaches the sidecar (within design decision 0029 and 0031)

- Prompt content stays game context only (design decision 0029): the chat request gains two optional fields, `liking` (the score as 0 to 1,
  two decimals, from `LikingPolicy::Percent`) and `relation` (the band word, `BandName`). The prompt carries the band word and the
  whole percent ("Your feeling for them: friendly, 62 of 100.") and one sentence for the band; no name, account, address,
  event count or date comes from the module. Both describe the pair the request already names (`bot_guid`, `player_guid`).
- No new requests and no new cost: the fields ride on the chat request that is sent anyway, so the shared $20 pool and the
  per-bot and per-human rate limits of design decision 0031 are unchanged. Memory events keep going as they do (they feed the sidecar's
  topics and last detail); only the score now comes from the module.
- The sidecar validates with `extra = "forbid"` (`schemas.py`), so the sidecar change ships first (section 12.1). Module side:
  `AwareAi.Chat.SendLiking` (default 0) switches the fields on once the sidecar knows them.
- One score in the end (owner, 2026-11-22): when a request carries the fields they are the single source of truth. The sidecar's
  own pair score (`pair_note.score`, -100 to 100) is kept and still fed by memory events, but only decides when the fields are
  absent. The event counts stay in the prompt as facts ("run dungeons with them 3 time(s)"), not as a second feeling.
- With the sidecar off the score still gates offers and invites; rule-based lines may read the band (cold bots do not greet
  warmly).

## 3. Playstyle (point 15)

- Five lasting styles from the bot's guid (`PlaystylePolicy::StyleFor`: a 64 bit mix, not a modulo, so neighbouring guids differ),
  with shares of 40 quester, 15 gatherer-crafter, 10 trader, 20 socializer, 15 PvPer (`AiPlayerbot.Social.Playstyle.Share.*`,
  only proportions count). No stored state: the same bot keeps its style while the shares stay the same. Changing the shares
  reshuffles the bots (accepted; a stored column would pin them, see gap 11).
- The style multiplies the weights of the `RpgStatusProbWeight` draw in percent (table `PlaystylePolicy::TABLE`; `Strength`
  scales the lean, 100 = table). A weight of 0 stays 0 (the owner's switch wins), any other weight stays at least 1. The
  availability checks of the draw still apply, so a bot without a gathering skill never draws gather.

| Style | Raises | Lowers | Reason |
|-------|--------|--------|--------|
| Quester | DoQuest 150 | WanderRandom, WanderNpc, GoGrind, GoCamp 70; OutdoorPvp, DuelSpot 60; DoGather 80 | Leveling by quests is its job |
| GathererCrafter | DoGather 300; GoGrind 130, TravelFlight 130 | DoQuest 80; OutdoorPvp, DuelSpot 50 | Nodes, mobs that drop materials, gathering routes |
| Trader | WanderNpc 200 (vendors, auction house, mailbox errands); TravelFlight 150; DoGather 120 | WanderRandom 60; DoQuest 70; OutdoorPvp, DuelSpot 50 | Markets and materials to sell |
| Socializer | DuelSpot 200, Rest 200 (the inn), WanderNpc 150, GoCamp 150, WanderRandom 120 | DoQuest, GoGrind 70; DoGather 60 | Where people are |
| PvPer | OutdoorPvp 300, DuelSpot 250; GoGrind 120 | DoQuest, WanderNpc, Rest 60; DoGather 50 | Fights for fun |

Every row keeps DoQuest at 60 or more (a test pins it): the leveling path of the random bots (design decision 0017) runs on quests.
City errands (`ErrandPolicy::Choose`) get the same idea in slice 2: the style scales the bank, auction, inn and mailbox
weights (trader: auction and mailbox up, socializer: inn up, gatherer-crafter: bank up). Traders also say sell lines more often
(section 5).

## 4. Gated offers (point 16)

1. **Group invites from a human** (exists): add `InviteChanceDelta` to the chance in `DecideInvite` and name "we have played
   together" in a warm decline of a friend (rule text rows `invite_decline_*_friend`, later).
2. **Shared-quest group offer** (built in slice 3, section 12.3; the text below is the design, the built values differ in the
   details listed there): a world-thread pass every 30 s over humans, like `BotGuilds::OfferToHumans`. A free random
   bot (no group, not in an instance or battleground, not dead or in combat) within 60 yards of the human, with the same quest in
   its log and not complete, level within the existing `LevelTolerance`, and liking at least 40, whispers the human and sends the
   ordinary group invite, which the human can decline. One offer per human from `OfferWindow` (2 per hour), a per-pair cooldown
   of 6 hours, never to a human who is in a group, DND, or has declined that bot in the last day. Text rows
   `social_quest_offer` with personality variants.
3. **Guild offer** (exists, off): `OfferToHumans` gains the gate `GuildOfferAllowed`. Today the leader of a recruiting guild
   near a guildless human whispers and invites; with liking on, the leader must like that human (55 or more), so a stranger is
   no longer approached by whoever stands near. One offer per human per hour (`GuildOffersPerHour`) on top of the existing 24 h
   cooldown, which moves from memory into the liking row's counters so a restart does not reset it. A friend bot who is only a
   member cannot send the invite (guild rights), so the alternative "the friend whispers, the leader invites" is an owner choice
   (question 2 of the hand-off).
4. Offers are not hand-outs: no gold, no items, no free levels (design decision 0016, 0017).

## 5. Trade and city chatter (point 13)

- Existing driver: `SuggestWhatToDoAction` (solo random bots) and `BroadcastHelper`. Slice 3 adds `TradeLinePolicy` (pure):
  a token bucket of `MaxPerHourPerFaction` (6) lines for each faction's city, `RequirePresence` (1): a line is said only while a
  human of that faction stands in the same city zone, and the sender is picked by playstyle (traders sell lines, gatherer-crafters
  recipe lines).
- Sell lines name an item the bot really listed (the economy track's AH listing record), with its listed price; recipe lines
  name up to `RecipesPerLine` (2) recipes the bot really knows (`BotProfessionProgress`). Without a real listing or recipe a bot
  says nothing: the generic `suggest_sell` line from bag contents stays as the fallback only while the economy track is off.
- Rule-based text only (design decision 0029: city and Trade chatter stays off the API). Config: `AiPlayerbot.Social.TradeLines.Enable` (0),
  `MaxPerHourPerFaction`, `RequirePresence`, `RecipesPerLine`.

## 6. Cities (point 17)

- Checked hub list: a table of 5.4.8 capital and faction-hub zones with the NPC kinds verified present (banker, auctioneer,
  innkeeper, mailbox, trainers), built by an audit against the world database; `ErrandPolicy::Town` already carries the flags. A
  hub not on the list gets no idle errands. No teleports: a bot reaches a hub on foot, by flight path or by its own Hearthstone
  (BT-253 and the stranded-bot rule already forbid unseen-free shortcuts).
- Errands that do real work: the bank errand deposits or withdraws (the economy track's reserve and commitment lists), the
  auction errand lists and buys (design decision 0033 points 1 to 7), the mailbox errand collects sold gold and expired items. Today they
  walk, stand and gesture only.
- Emotes (built in slice 3, section 12.2): random bots near a human answer an emote aimed at them with a gesture from the reaction
  table, at most once per human per minute. Rude emotes do not feed the Insulted event (not built; the tone check works on text).
- No world events (design decision 0033 point 18).

## 7. Privacy, retention, wipe

- The liking table holds two guids, a number and timestamps, no names. A human guid is personal data in the owner's sense: it can
  be traced to a character.
- Retention: `RetentionDays` (180). A startup and daily cleanup deletes rows not changed for that long whose decayed value is
  within 2 points of neutral (`LikingPolicy::Forgettable`; after 180 days, about eight half-lives, every row qualifies).
- Deleting a character removes its rows (a `PlayerScript::OnDelete` hook for a human guid and for a bot guid).
- Wipe command (GM console, not a chat command; the bots have no command layer): `.playerbots social wipe <character name>`
  deletes every row with that `player_guid` in the module table and asks the sidecar to delete that guid's memory
  (`POST /v1/memory/wipe`, section 12.1); `.playerbots social wipe all` does it for every human. Returns the counts. The sidecar
  side covers `memory_event`, `chat_turn`, `social_event`, `pair_note` and the JSONL log lines that name the human.
- The sidecar prompt carries the band word and the percent only, never a count of events or the dates.
- Nothing is logged with a score at info level (debug only) unless `AiPlayerbot.Social.SoakLog = 1` (section 13: bots by name, humans
  by low guid only), and the events log of the sidecar gets the band, not the guid.

## 8. Slice 2 hook points

| Feature | File:function |
|---------|---------------|
| Load and flush the table, create it at startup like the arena store | new `src/Manager/LikingStore.{h,cpp}`, called from `PlayerbotsWorldScript::OnStartup` and `OnUpdate` (`mod_playerbots.cpp:79`, `:176`); human login and logout from `PlayerbotsPlayerScript` (`mod_playerbots.cpp:306`) |
| Group-time pass | `RandomPlayerbotMgr::UpdateAIInternal` next to `BotGuilds::Update()` (`RandomPlayerbotMgr.cpp:206`) |
| Dungeon done | `LfgMannersAction.cpp:283`, beside `AiChat::OnDungeonDone` |
| Traded | `TradeStatusAction::Execute`, `TRADE_STATUS_ACCEPTED` branch (`TradeStatusAction.cpp:229`) |
| Dueled | `PlayerbotsPlayerScript::OnDuelEnd` (`mod_playerbots.cpp:447`) |
| Guild time | `BotGuilds::Update` pass (`BotGuilds.cpp:579`) |
| Thanked, insulted | `AiChat::NoteTone` (`AiChat.cpp:877`), moved ahead of the `ChatOn()` test |
| Kicked | new `PlayerbotsGroupScript::OnRemoveMember` override (`mod_playerbots.cpp:498`) |
| Loot ninja | `AiChat::OnLootRollWon` (`AiChat.cpp:1689`) |
| Invite chance | `DecideInvite`, after the guild bonus (`AcceptInvitationAction.cpp`, just after `chance += config.guildBonus`) |
| Guild offer gate | `OfferToHumans` (`BotGuilds.cpp:493`) |
| Quest group offer | new pass beside `BotGuilds::Update` (`RandomPlayerbotMgr.cpp:206`) |
| Playstyle weights | `NewRpgBaseAction::RandomChangeStatus` (`NewRpgBaseAction.cpp:2362`): replace the three reads of `RpgStatusProbWeight[status]` by one helper that applies `PlaystylePolicy::ScaledWeight` when `socialPlaystyleEnable` |
| Errand weights | `ErrandPolicy::Choose` call (`NewRpgAction.cpp:959`) |
| Sidecar fields | `AwareAiChatRequest` and `BuildChatBody` (`AwareAiProtocol.{h,cpp}`), filled where `request.Sociability` is set (`AiChat.cpp:490`) |
| Wipe command | `cs_playerbots.cpp` |

## 9. Gaps, in order

| # | Gap | Size | Depends on |
|---|-----|------|-----------|
| 1 | Liking store: create the table at startup (the module has no SQL updater), per-human load, write-behind flush, cleanup job, tests of the pure parts | S to M | slice 1 |
| 2 | Event hooks (group-time pass, dungeon, trade, duel, guild time, tone, kick, ninja), each a one-line call into the store | M | 1 |
| 3 | Invite chance delta in `DecideInvite` | S | 1 |
| 4 | Playstyle in `RandomChangeStatus`, a per-bot style cache read once per draw, a debug line `[SOCIAL] style` | S | slice 1 |
| 5 | Wipe command and character-delete hook, retention cleanup | S | 1 |
| 6 | Guild offer gate and a persistent human cooldown | S | 1 |
| 7 | Sidecar: accept `liking` and `relation`, use them for the prompt and the friend whisper, a wipe endpoint, schema export and tests; then `AwareAi.Chat.SendLiking` in the module | M (two code bases) | 1 (done in slice 3, live check pending) |
| 8 | Shared-quest group offer pass, text rows, `OfferWindow` per human | M | 1, 3 (done in slice 3, live check pending) |
| 9 | Errand weights by playstyle, then the checked hub list (a world-database audit) | M | 4 |
| 10 | Trade lines: `TradeLinePolicy`, presence gate, real listings and recipes | M | economy track (listings, recipes) |
| 11 | Pin a bot's style in a column when the owner changes shares (optional) | S | 4 |
| 12 | Emote replies for random bots near a human, per-human limit | S to M | 2 (done in slice 3, live check pending) |
| 13 | Metrics for the training loop: offers per hour, accepted share, liking histogram, style census, trade lines per hour | S | 2, 8, 10 |
| 14 | Overnight soak with all switches on, then the defaults | gate | all |

## 10. Tests

`tests/test_liking_policy.cpp` and `tests/test_playstyle_policy.cpp`, run by `tests/run.sh` (the harness lists every
`tests/test_*.cpp` file): deltas, bounds, the daily cap, decay from both sides, bands and gates, the hourly offer window,
retention, style shares and stability, the table's leans, the quest floor, strength, zero weights.

## 11. Slice 2: what is wired (all behind the two switches, default 0)

With `AiPlayerbot.Social.Liking.Enable = 0` and `AiPlayerbot.Social.Playstyle.Enable = 0` every hook below returns at once and the
playstyle helper returns the global weight: nothing is read or written. Owner answers of 2026-11-22: daily cap 15 points, guild
offers only from the leader with liking 55 or more and at most one per human per hour, the playstyle comes from the guid hash only
(no stored column), the kick penalty is -8 and ignored in the first 2 minutes, the sidecar fields wait for the sidecar change.

### 11.1 The store (`src/Manager/LikingStore.{h,cpp}`, pure side `src/Utils/LikingBook.h`)

- Tables: `playerbots_social_liking` and `playerbots_social_offer` (last guild offer per human, kind 1) are created with
  `CREATE TABLE IF NOT EXISTS` by `EnsureTablesLocked` on first use (login, first pass, an event, a wipe or a deleted character);
  the same text is in `sql/playerbots/updates/db_playerbots/2026_11_22_00.sql` and `2026_11_22_01.sql` for fresh installs.
- Load: `OnHumanLogin` (from `PlayerbotsPlayerScript::OnLogin`) and the minute pass begin a human in the `Book` and ask two async
  queries (rows by `player_guid`, offer time). `Update` takes the answers in (`PollLoadsLocked`) and asks again after 30 s.
  Events that come before the rows are in wait (at most 64) and replay in order on the loaded rows. Nothing reads the database on a
  bot's tick; until a human is loaded the invite delta is 0 and a guild offer is refused.
- Write: dirty rows go out once a minute (`FlushLocked`, one multi-row `REPLACE` per 100 rows, on the async queue, which keeps
  order), at logout (`OnHumanLogout`, also with the switch off by then) and at shutdown (`Shutdown`, from
  `PlayerbotsWorldScript::OnShutdown`, direct writes).
- Retention: `RetentionSql` runs at the first tick and then daily: it deletes rows older than `RetentionDays` whose decayed value is
  within `FORGET_DUST` of neutral (the SQL form of `LikingPolicy::Forgettable`) and offer times older than that.
- Thread safety: one mutex around the book; the hooks run on the world, session and map threads and only touch memory.

### 11.2 Hook points

| Feature | File:function |
|---------|---------------|
| Minute pass (group time, guild time, flush, retention, loads) | `RandomPlayerbotMgr.cpp` `UpdateAIInternal`, after `BotGuilds::Update()` calls `LikingStore::Update()` |
| Login, logout, shutdown | `mod_playerbots.cpp` `PlayerbotsPlayerScript::OnLogin` and `OnLogout`, `PlayerbotsWorldScript::OnShutdown` |
| Deleted character | `mod_playerbots.cpp` `PlayerbotsPlayerScript::OnDelete` (`LikingStore::OnPlayerDeleted`), rows as human and as bot |
| Dungeon done | `LfgMannersAction.cpp`, beside `AiChat::OnDungeonDone` (`LikingStore::OnDungeonDone`) |
| Traded | `TradeStatusAction::Execute`, the `TRADE_STATUS_ACCEPTED` branch: when the bot's accept closes the trade and either side offered an item or gold |
| Dueled | `PlayerbotsPlayerScript::OnDuelEnd`, any end but `DUEL_INTERRUPTED` |
| Thanked, insulted | `LikingStore::OnHumanLine`, called from the whisper, say and group `OnChat` hooks in `mod_playerbots.cpp`; works with the AI layer off (the AI layer's own `NoteTone` is unchanged and still feeds the sidecar). The line counts for the whispered bot, a group bot whose name is in the line, or a random bot within 40 yd that the human has targeted; the same tone from one pair counts once a minute |
| Kicked | `PlayerbotsGroupScript::OnRemoveMember` (new) and `OnAddMember` (join time); a kick by a human of a random bot, method kick or Finder vote kick, counts unless the bot had been in the group less than `KickGraceSeconds`; a bot with no known join time counts |
| Loot ninja | `AiChat::OnLootRollWon` (and `NoteLootRoll`, the wish book), now active when liking is on even if the AI layer is off |
| Invite chance | `AcceptInvitationAction.cpp` `DecideInvite`, after `chance += config.guildBonus`, inside the existing clamp |
| Guild offer gate | `BotGuilds.cpp` `OfferToHumans`, before the offer text: `LikingStore::MayOfferGuild` (liking, persistent cooldown from `HumanInviteCooldownHours`, hour window) |
| Playstyle weights | `NewRpgBaseAction.cpp` `DrawWeight`, used by the four reads in `RandomChangeStatus`; debug line `[SOCIAL] style` |
| Wipe command | `cs_playerbots.cpp` `HandleSocialCommand` -> `LikingStore::Console` |

### 11.3 Wipe and deletion

`.playerbots social wipe <character name>` (GM permission, console or in game) counts and deletes the character's rows in both
tables as a human and as a bot, and clears its memory; `.playerbots social wipe all` does it for every character. The delete
statements go on the same async queue as the flushes, so they run after any write queued before them. The same removal runs when a
character is deleted. Both work with the switch off and create the empty tables if they are missing. The sidecar's own copy of the
history is a separate endpoint (gap 7).

### 11.4 Known limits

- The tone hook covers whispers, say and party or raid lines, not guild chat or channels.
- Group time counts every random bot in the human's group on any map; it stops in a battleground group and during a battleground
  queue.
- A pair's row exists only after an event, so a human who never grouped or traded is never offered a guild by liking (intended).
- The in-memory hour window of guild offers resets at a restart; the persistent cooldown covers the long run.

## 12. Slice 3: sidecar fields and wipe, emote replies, shared-quest offers (all behind switches, default 0)

Owner answers of 2026-11-22: the sidecar change ships first and the module switch `AwareAi.Chat.SendLiking` follows; emote replies
for random bots near a human, at most 1 per human per minute; liking-gated quest-help group offers capped per human per hour (2).

### 12.1 The sidecar contract (`services/aware-ai`, schemas in `schemas/v1/`)

| Item | Detail |
|------|--------|
| `chat_request.liking` | optional number 0 to 1; the module sends the whole percent divided by 100 |
| `chat_request.relation` | optional `cold`, `stranger`, `friendly` or `close` (the band the module computed with its own edges) |
| Which wins | `relation` over `liking`; `liking` alone is turned into a band with the default edges 0.20, 0.55 and 0.75; neither: the sidecar's own pair score decides, as before |
| Prompt | the band sentence ("You like them.", "You do not like them and keep your distance.", nothing for a stranger, "You like them a lot and count them as a close friend.") plus "(Your feeling for them: friendly, 62 of 100.)"; with no fields the old "You like them." at an own score of 20 or more and the grudge line below 0 |
| `friend_whisper` gate | band friendly or close when the fields are sent (own score ignored), own score 20 or more otherwise; the 12 hour cooldown applies on top |
| Wipe | `POST /v1/memory/wipe` with `{"schema_version": 1, "player_guid": N}` or `{"schema_version": 1, "all": true}` (exactly one); the answer holds the counts (`memory_events`, `chat_turns`, `social_events`, `pair_notes`, `log_lines`) |
| What a wipe removes | those four tables for the guid, the JSONL log lines whose `player_guid` or `target_guid` is the guid (chat lines keep the human's name and words), the guid's entries in the rate and cooldown books; never the spend ledger or the settings |
| Module call | `AiChat::WipeSidecar` from `LikingStore::Console` (`.playerbots social wipe <name>|all`) when `AwareAi.Enable = 1`: fire and forget, counts and failures go to the server log (`[AIWIPE]`), the console line says when nothing was sent (layer off, link down, client busy) so the operator can run it again |

The module sends the full guid (`Player::GetGUID()`), the same value every other request uses; the wipe uses
`ObjectMgr::GetPlayerGUIDByName`, which builds the same value. A wipe never sends a name.

### 12.2 Emote replies (`AiPlayerbot.Social.EmoteReplies.*`, `EmoteReplyPolicy.h`)

- Switch `AiPlayerbot.Social.EmoteReplies.Enable` (0). With it on, `EmoteStrategy` registers the `receive text emote` trigger even when
  `AiPlayerbot.RandomBotEmote` is 0 (the idle chatter stays off), and `EmoteAction::Execute` sends the text emote of a human to
  `ReplyToHumanEmote` when the bot is a random bot. AC's reactions (`RandomBotEmote = 1`) stay for the other bots and for a human's
  alt bots; with `RandomBotEmote = 0` they do nothing, as before.
- `EmoteReplyPolicy::Decide`, in this order: an emote aimed at someone else is not ours; farther than `Radius` (20) is too far, and an
  emote with no target needs the bot within `UntargetedRadius` (8); a busy bot (dead, in combat, in flight, mounted, moving, casting,
  in a battleground, arena, queue or instance, being teleported) stays out; a cold bot (liking band cold, only when the liking switch
  is on) ignores the human; an untargeted emote needs a roll of `UntargetedChance` (35 percent); and last the per-human limit.
- The limit is one reply per human per `PerHumanSeconds` (60) across all bots, kept in memory (a restart forgets it). It is taken
  only after a gesture really played: the emote table has no answer for every emote, and an unanswered one does not use up the
  minute. Several bots that see the same emote in one tick: the first to act takes the minute.
- The gesture is `EmoteAction::ReceiveEmote(..., verbal = false, social = true)`: the AC table as it is (about 150 cases), without
  the spoken or yelled lines and without the follow and stay cases that act for a master; the bot faces the human and the text emote
  is always aimed at them (AC aims it half of the time). A bot that is walking is busy, so nobody stops mid-route to wave (owner rule:
  no stop-and-go).
- Known limits: only the text emote packet (`/wave`, `/dance`, ...) counts, not the animation packet; a rude emote is answered with
  the table's gesture (a bite is answered with a roar) and does not lower the liking; the table's gestures are not liking-aware
  beyond the cold gate.

### 12.3 Shared-quest group offers (`AiPlayerbot.Social.QuestOffer.*`, `QuestOfferPolicy.h`, `Manager/SocialOffers.{h,cpp}`)

- Switches: `AiPlayerbot.Social.QuestOffer.Enable` (0) and `AiPlayerbot.Social.Liking.Enable`; both must be 1. The owner asked for
  the pass to sit behind the liking switch; its own switch is added because a bot that invites humans is more visible than a score
  that only counts, so the score can be soaked first. Hand-off question 3.
- One pass every `PassSeconds` (30) on the world thread, called from `RandomPlayerbotMgr::UpdateAIInternal` after
  `LikingStore::Update`. For each human who is free (in the world, alive, not in combat, flight, an instance, battleground, arena or
  queue, not DND, level 5 or more, no pending invite, not in a group with others) and has an unfinished quest, every random bot on
  the same map and team within `Radius` (60) is tested by `QuestOfferPolicy::Decide`: the bot is free and in no group with others,
  same zone, level within `LevelTolerance` (5), and an unfinished quest (status incomplete, not complete) that is in both logs. The
  candidates are tried nearest first (ties by guid), the first that passes `LikingStore::MayOfferQuestGroup` makes the offer.
- `MayOfferQuestGroup` = `Book::TakeQuestOffer`: the human's rows are loaded, liking is at least `QuestOfferMin` (40), the same bot
  did not offer this human in the last `PairCooldownHours` (6), and the human's hour window has room (`QuestOffersPerHour`, 2, from all
  bots together). A pair that never shared a moment sits at neutral 30 and is never offered a group (intended: the bot has to know
  the human). The windows and pair cooldowns are in memory (a restart or a logout forgets them).
- The offer: the ordinary group invite (`InviteToGroupAction::Invite`, the human may decline) and a whisper from the text table
  (`social_quest_offer`, a variant per personality, `2026_11_22_02.sql`, with a code fallback) naming the quest as a link. The invite
  goes first; when it cannot be sent nothing is said. Never in combat, an instance or a battleground, for either side.
- Not built: remembering that a human declined (the pair cooldown covers it), a group offer to a human who is already grouped, quest
  sharing, any gold or item. Offers are not hand-outs (design decision 0016).

### 12.4 Hook points

| Feature | File:function |
|---------|---------------|
| Sidecar fields | `AiChat.cpp` `FillLiking`, called in `Send` after `request.Conversation`; `AwareAiProtocol.cpp` `BuildChatBody` |
| Sidecar wipe | `AiChat::WipeSidecar`, called from `LikingStore::Console`; `BuildWipeBody` and `ParseWipeBody` in `AwareAiProtocol.cpp` |
| Emote replies | `EmoteAction.cpp` `Execute` (the `SMSG_TEXT_EMOTE` branch) -> `ReplyToHumanEmote`; trigger in `EmoteStrategy.cpp` `InitTriggers` |
| Quest offers | `RandomPlayerbotMgr.cpp` `UpdateAIInternal` -> `SocialOffers::Update` |
| Config | `PlayerbotAIConfig.cpp` (`socialQuestOffer`, `socialEmoteReplies`), `conf/playerbots.conf.dist` |

### 12.5 Tests and live checks

Unit tests: `tests/test_emote_reply_policy.cpp`, `tests/test_quest_offer_policy.cpp`, the quest offer tests in
`tests/test_liking_book.cpp`, the body tests in `tests/aware_ai/test_aware_ai.cpp`; sidecar: `tests/test_liking_and_wipe.py`.
Live checks (not done, no build in this slice): with the sidecar updated and `AwareAi.Chat.SendLiking = 1`, a whisper to a friend bot
shows the "Your feeling for them" line in the sidecar's logged context and a stranger bot refuses a `friend_whisper`; `.playerbots
social wipe <name>` prints the sidecar line and the server log shows `[AIWIPE]` counts; `/wave` at a targeted bot in a city gets a
gesture back once a minute and a second wave inside the minute gets none; two humans in one zone on the same quest as a bot with
liking 40 or more get one whisper and invite, not more than two in an hour.

## 13. Soak support: the OnlyBots scope, log lines and the summary (owner approved the overnight soak on named test bots, 2026-11-23)

Pure logic in `src/Utils/SocialSoakPolicy.h` (tests: `tests/test_social_soak_policy.cpp`), the game side in `src/Manager/SocialLog.{h,cpp}`.

### 13.1 Owner answers of 2026-11-22: where each one lives

| Answer | Where |
|--------|-------|
| `social wipe all` also clears the sidecar | `LikingStore::Console` calls `AiChat::WipeSidecar(std::nullopt)`; the sidecar endpoint takes `{"all": true}` |
| A lost sidecar wipe is reported on the console, no retry | `AiChat::WipeSidecar` returns "NOT wiped" lines (link down, client busy) and logs `[AIWIPE]`; there is no queue and no pending flag |
| Quest offers need both switches | `SocialOffers::Update` and `LikingStore::MayOfferQuestGroup` (both `QuestOffer.Enable` and `Liking.Enable`) |
| Rude emotes do not lower liking, typed insults do | no `LikingPolicy::Event` for an emote; only `LikingStore::OnHumanLine` (`AiChatLines::ToneOf`) feeds `Insulted` |
| Untargeted emotes: 35 percent within 8 yd | `EmoteReplyPolicy::Tuning` (`untargetedChancePct`, `untargetedYards`) |
| Guild leader at 55 or more | `LikingPolicy::Tuning::guildOfferMin`, `LikingStore::MayOfferGuild` (the leader of the guild is the only one asked) |
| Kick at -8 with a grace | `Delta.Kicked` -8, `KickGraceSeconds` 120, `LikingPolicy::KickCounts` |
| "15 per day cap" | the daily cap on positive moves of a pair, 15 points (`DailyGainCap`); offers have no per-day count, only per hour (quest 2, guild 1) and the pair cooldown. Asked in the hand-off |
| Sidecar first, local fallback | with `AwareAi.Chat.SendLiking` the score travels with the chat request and decides the prompt; with the AI layer off, the link down or the rows not loaded the sidecar keeps its own score and the module's gates (offers, invites) run on the local score alone |

### 13.2 The OnlyBots scope (`AiPlayerbot.Social.OnlyBots`, default empty)

A comma, semicolon or space separated list of bot names, not case sensitive. Empty: no limit, every behaviour as before. Not empty:
only the listed bots take part in anything social, every other bot behaves as with the switches off:

| Behaviour | Where the scope is tested |
|-----------|---------------------------|
| every liking event (group time, guild time, dungeon, trade, duel, thanks, insult, kick, ninja), the score for invites and the sidecar | `LikingStore.cpp` `IsRandomBot` (one function for all paths) |
| guild offers | `LikingStore::MayOfferGuild`, first test, also with the liking switch off |
| shared-quest offers | `QuestOfferPolicy::Facts::inScope`, `Verdict::OutOfScope` (last test, so only a bot that would have offered is counted) |
| emote replies | `EmoteReplyPolicy::Facts::inScope`, `Verdict::OutOfScope` (after the aim and range tests) |
| playstyle weights | `NewRpgBaseAction.cpp` `DrawWeight`: an out-of-scope bot draws from the global table |

### 13.3 Log lines (grep keys) and the summary

| Line | Level | Fields |
|------|-------|--------|
| `[SOCIAL] liking bot=<name> human=<guid> event=<name> steps=<n> result=up\|down\|capped\|queued before=<0-100> after=<0-100>` | info with `SoakLog = 1`, else debug | one per applied event (group and guild time included) |
| `[SOCIAL] emote bot=<name> human=<guid> text_emote=<id> aim=bot\|nobody yards=<n>` | info with `SoakLog = 1`, else debug | one per gesture that played |
| `[SOCIAL] offer kind=quest\|guild bot=<name> human=<guid> quest=<id> result=made` | info always | one per offer made (the guild offer also keeps its `[BOTGUILD]` line) |
| `[SOCIAL] summary scope=<all bots\|N named bots> window=<s>s liking_up=.. liking_down=.. ... out_of_scope=..` | info | totals since the server started, every `SummarySeconds` (900) when a counter changed, and once at shutdown |
| `[SOCIAL] config: ... scope=.. soaklog=.. summary=..s` | info | once at start and at every config reload |

Humans appear as the low guid only. The summary counters: `liking_up liking_down liking_capped liking_queued emote_reply
emote_no_gesture emote_chance emote_limited emote_cold emote_busy quest_offer quest_offer_refused guild_offer guild_offer_refused
out_of_scope`. Refusals are counted once per human and pass (quest) or per ask (guild), never logged line by line.
`liking_queued` is an event that arrived before the human's rows loaded (it applies on load; the line shows no score).

### 13.4 Live checks for the soak

With a human (or a test client) online near the test bots: a `/wave` at a test bot gives one `[SOCIAL] emote` line and `emote_reply`
up by one; the same wave at a bot not on the list leaves `out_of_scope` up and no line; thanks typed to a test bot gives one
`[SOCIAL] liking ... event=thanked`. Without a human online the counters stay at zero: nothing social starts on its own.
