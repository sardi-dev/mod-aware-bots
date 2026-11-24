/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * The liking store (see LikingStore.h and docs/social/social.md). The module has no SQL updater for the playerbots
 * database, so the tables are created here with CREATE TABLE IF NOT EXISTS before the first use (the same text is in
 * sql/playerbots/updates/ for fresh installs), like the arena store does. Statements are plain strings built from numbers
 * (LikingBook.h); no name or text from a player goes into one. Writes go to the database's asynchronous queue, which runs
 * them in the order they were queued, so a later delete always lands after an earlier flush.
 */

#include "LikingStore.h"

#include "AiChat.h"
#include "AiChatLines.h"
#include "AiChatPolicy.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "LikingBook.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "PlayerbotsDatabase.h"
#include "RandomPlayerbotMgr.h"
#include "SocialLog.h"
#include "SocialSoakPolicy.h"
#include "WorldSession.h"

#include <algorithm>
#include <ctime>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace
{
    char const* const CREATE_LIKING =
        "CREATE TABLE IF NOT EXISTS `playerbots_social_liking` ("
        "`bot_guid` INT UNSIGNED NOT NULL,"
        "`player_guid` INT UNSIGNED NOT NULL,"
        "`value` FLOAT NOT NULL DEFAULT 0.3,"
        "`updated_at` INT UNSIGNED NOT NULL DEFAULT 0,"
        "`gain_day` INT UNSIGNED NOT NULL DEFAULT 0,"
        "`gain_today` FLOAT NOT NULL DEFAULT 0,"
        "PRIMARY KEY (`bot_guid`, `player_guid`),"
        "KEY `idx_social_liking_player` (`player_guid`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

    char const* const CREATE_OFFER =
        "CREATE TABLE IF NOT EXISTS `playerbots_social_offer` ("
        "`player_guid` INT UNSIGNED NOT NULL,"
        "`kind` TINYINT UNSIGNED NOT NULL,"
        "`last_at` INT UNSIGNED NOT NULL DEFAULT 0,"
        "PRIMARY KEY (`player_guid`, `kind`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

    char const* const CREATE_QUEST_OFFER =
        "CREATE TABLE IF NOT EXISTS `playerbots_social_quest_offer` ("
        "`id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "`player_guid` INT UNSIGNED NOT NULL,"
        "`bot_guid` INT UNSIGNED NOT NULL,"
        "`offered_at` INT UNSIGNED NOT NULL,"
        "PRIMARY KEY (`id`),"
        "KEY `idx_quest_offer_player` (`player_guid`),"
        "KEY `idx_quest_offer_bot` (`bot_guid`),"
        "KEY `idx_quest_offer_at` (`offered_at`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

    // Seconds between the passes that credit group and guild time and flush the changed rows.
    constexpr uint32 PASS_SECONDS = 60;
    constexpr uint32 GUILD_STEP_SECONDS = LikingPolicy::SECONDS_PER_HOUR;
    // A load whose answer has not come in this long is asked again.
    constexpr uint32 LOAD_RETRY_SECONDS = 30;
    // The same thanks or insult from one pair counts once in this window.
    constexpr uint32 TONE_WINDOW_SECONDS = 60;
    // A line to a bot the human has only targeted counts when the bot is this close.
    constexpr float TARGET_LINE_YARDS = 40.0f;
    constexpr size_t FLUSH_ROWS_PER_PASS = 1000;
    constexpr size_t JOIN_BOOK_LIMIT = 4096;

    struct Loading
    {
        QueryResultFuture rows;
        QueryResultFuture offers;
        QueryResultFuture questOffers;
        uint32 askedAt = 0;
    };

    std::mutex storeLock;
    LikingBook::Book book;
    LikingBook::RepeatGate thanksGate;
    LikingBook::RepeatGate insultGate;
    std::unordered_map<uint32, uint32> joinedAt;               // bot low guid -> when it joined its group
    std::unordered_map<uint32, std::unique_ptr<Loading>> loading;  // human low guid -> its pending load

    // World thread only (Update).
    LikingBook::PairClock groupClock;
    LikingBook::PairClock guildClock;
    uint32 lastTickAt = 0;
    uint32 nextPassAt = 0;
    uint32 lastPassAt = 0;
    uint32 nextRetentionAt = 0;

    bool tablesReady = false;
    bool shuttingDown = false;

    uint32 Now()
    {
        return uint32(time(nullptr));
    }

    LikingPolicy::Tuning const& Tuning()
    {
        return sPlayerbotAIConfig->socialLiking;
    }

    bool IsHumanPlayer(Player* player)
    {
        return player && player->GetSession() && !player->GetSession()->IsBot() && !GET_PLAYERBOT_AI(player);
    }

    // A module bot of the random accounts (not a player's own alt bot, which sits on the player's account).
    bool IsRandomAccountBot(Player* player)
    {
        if (!player || !player->GetSession() || !player->GetSession()->IsBot())
            return false;

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(player);
        return botAI && !botAI->IsRealPlayer() && sPlayerbotAIConfig->IsInRandomAccountList(player->GetSession()->GetAccountId());
    }

    // The bots that take part in anything social: random bots on the AiPlayerbot.Social.OnlyBots list, or all of them while
    // the list is empty. Every liking path asks this one function, so the soak limit holds for all of them.
    bool IsRandomBot(Player* player)
    {
        return IsRandomAccountBot(player) && SocialLog::InScope(player);
    }

    // A pair that is a random bot and a human but was left out because the bot is not on the list; counted for the summary.
    bool LeftOutByScope(Player* first, Player* second)
    {
        return (IsRandomAccountBot(first) && IsHumanPlayer(second) && !SocialLog::InScope(first)) ||
            (IsRandomAccountBot(second) && IsHumanPlayer(first) && !SocialLog::InScope(second));
    }

    // The bot and the human of a pair given in either order; false when it is not a random bot and a human.
    bool Resolve(Player* first, Player* second, Player*& bot, Player*& human)
    {
        if (IsRandomBot(first) && IsHumanPlayer(second))
        {
            bot = first;
            human = second;
        }
        else if (IsRandomBot(second) && IsHumanPlayer(first))
        {
            bot = second;
            human = first;
        }
        else
            return false;

        return LikingPolicy::PairEligible(true, true, bot->GetGUIDLow(), human->GetGUIDLow());
    }

    // One event for a pair, under the lock: applies it, counts the outcome and writes the action line (docs/social/social.md
    // section 13). Before and after are the score around the event; with the human's rows still loading the event waits.
    void RecordLocked(Player* bot, uint32 humanLow, LikingPolicy::Event event, uint32 steps, uint32 now)
    {
        LikingPolicy::Tuning const& tuning = Tuning();
        bool const loaded = book.Loaded(humanLow);
        double const before = book.Value(tuning, humanLow, bot->GetGUIDLow(), now);
        book.Note(tuning, humanLow, bot->GetGUIDLow(), event, steps, now);
        double const after = book.Value(tuning, humanLow, bot->GetGUIDLow(), now);

        SocialSoak::Move const move = SocialSoak::ClassifyMove(loaded, LikingPolicy::DeltaOf(tuning, event) * double(steps), before, after);
        SocialLog::Count(SocialSoak::MoveCounter(move));
        SocialLog::Action(SocialSoak::LikingLine(bot->GetName(), humanLow, event, steps, move, before, after));
    }

    // Creates the tables once (idempotent), under the lock.
    void EnsureTablesLocked()
    {
        if (tablesReady)
            return;

        PlayerbotsDatabase.DirectExecute(CREATE_LIKING);
        PlayerbotsDatabase.DirectExecute(CREATE_OFFER);
        PlayerbotsDatabase.DirectExecute(CREATE_QUEST_OFFER);
        tablesReady = true;
        SF_LOG_INFO("playerbots", "[SOCIAL] liking tables ready");
    }

    // The human's rows and offer time are asked for on the database's async threads; Update takes the answers in.
    void AskLoadLocked(uint32 human, uint32 now)
    {
        std::unique_ptr<Loading>& entry = loading[human];
        if (!entry)
            entry = std::make_unique<Loading>();
        else
        {
            entry->rows.cancel();
            entry->offers.cancel();
            entry->questOffers.cancel();
        }

        entry->askedAt = now;
        entry->rows = PlayerbotsDatabase.AsyncPQuery(
            "SELECT `bot_guid`, `value`, `updated_at`, `gain_day`, `gain_today` FROM `playerbots_social_liking` "
            "WHERE `player_guid` = %u", human);
        entry->offers = PlayerbotsDatabase.AsyncPQuery(
            "SELECT `last_at` FROM `playerbots_social_offer` WHERE `player_guid` = %u AND `kind` = %u", human,
            LikingBook::OFFER_KIND_GUILD);
        uint32 const span = std::max(sPlayerbotAIConfig->socialQuestOffer.pairCooldownSeconds, LikingPolicy::SECONDS_PER_HOUR);
        uint32 const cutoff = now > span ? now - span : 0;
        entry->questOffers = PlayerbotsDatabase.AsyncPQuery(
            "SELECT `bot_guid`, `offered_at` FROM `playerbots_social_quest_offer` WHERE `player_guid` = %u AND `offered_at` >= %u",
            human, cutoff);
    }

    void BeginHumanLocked(uint32 human, uint32 now)
    {
        EnsureTablesLocked();
        if (book.Has(human))
            return;

        book.Begin(human);
        AskLoadLocked(human, now);
    }

    // Takes in the loads that came back and asks again for those that did not.
    void PollLoadsLocked(uint32 now)
    {
        for (auto itr = loading.begin(); itr != loading.end();)
        {
            Loading& entry = *itr->second;
            if (entry.rows.ready() && entry.offers.ready() && entry.questOffers.ready())
            {
                QueryResult rowsResult;
                QueryResult offersResult;
                QueryResult questResult;
                entry.rows.get(rowsResult);
                entry.offers.get(offersResult);
                entry.questOffers.get(questResult);

                std::vector<LikingBook::Row> rows;
                if (rowsResult)
                {
                    do
                    {
                        Field* f = rowsResult->Fetch();
                        LikingBook::Row row;
                        row.bot = f[0].GetUInt32();
                        row.human = itr->first;
                        row.pair.value = LikingPolicy::Clamp01(f[1].GetFloat());
                        row.pair.updatedAt = f[2].GetUInt32();
                        row.pair.gainDay = f[3].GetUInt32();
                        row.pair.gainToday = f[4].GetFloat();
                        rows.push_back(row);
                    } while (rowsResult->NextRow());
                }

                uint32 const offerAt = offersResult ? offersResult->Fetch()[0].GetUInt32() : 0;
                std::vector<LikingBook::QuestOffer> questOffers;
                if (questResult)
                {
                    do
                    {
                        Field* f = questResult->Fetch();
                        questOffers.push_back({ itr->first, f[0].GetUInt32(), f[1].GetUInt32() });
                    } while (questResult->NextRow());
                }
                book.Load(Tuning(), itr->first, rows, offerAt, questOffers);
                itr = loading.erase(itr);
            }
            else if (now >= entry.askedAt + LOAD_RETRY_SECONDS)
            {
                SF_LOG_WARN("playerbots", "[SOCIAL] a liking load has not answered in %u s, asked again", LOAD_RETRY_SECONDS);
                AskLoadLocked(itr->first, now);
                ++itr;
            }
            else
                ++itr;
        }
    }

    // Writes rows and offer times. The async queue keeps them in order; a stopping server writes directly.
    void WriteLocked(std::vector<LikingBook::Row> const& rows, std::vector<std::pair<uint32, uint32>> const& offers)
    {
        auto run = [](std::string const& sql)
        {
            if (shuttingDown)
                PlayerbotsDatabase.DirectExecute(sql.c_str());
            else
                PlayerbotsDatabase.Execute(sql.c_str());
        };

        for (size_t first = 0; first < rows.size(); first += LikingBook::ROWS_PER_STATEMENT)
            run(LikingBook::ReplaceSql(rows, first, std::min(rows.size(), first + LikingBook::ROWS_PER_STATEMENT)));
        for (auto const& [human, offerAt] : offers)
            run(LikingBook::OfferReplaceSql(human, offerAt));
    }

    void FlushLocked()
    {
        std::vector<std::pair<uint32, uint32>> offers;
        std::vector<LikingBook::Row> const rows = book.TakeDirty(0, FLUSH_ROWS_PER_PASS, &offers);
        if (!rows.empty() || !offers.empty())
            WriteLocked(rows, offers);
    }

    void ExecuteAll(std::vector<std::string> const& statements)
    {
        for (std::string const& sql : statements)
            PlayerbotsDatabase.Execute(sql.c_str());
    }

    void RetentionLocked(uint32 now)
    {
        ExecuteAll(LikingBook::RetentionSql(Tuning(), now, sPlayerbotAIConfig->socialLikingRetentionDays));
        PlayerbotsDatabase.Execute(LikingBook::QuestRetentionSql(now, sPlayerbotAIConfig->socialLikingRetentionDays,
            sPlayerbotAIConfig->socialQuestOffer.pairCooldownSeconds).c_str());
    }

    // The once-a-minute pass: humans the book does not know yet, then group time and guild time.
    void SocialPassLocked(uint32 now)
    {
        uint32 const credit = LikingBook::PassCredit(lastPassAt, now);
        lastPassAt = now;

        LikingPolicy::Tuning const& tuning = Tuning();
        uint32 const groupStepSeconds = tuning.groupStepMinutes * 60;
        std::unordered_map<uint32, std::vector<Player*>> botsByGuild;
        bool guildsBuilt = false;

        for (Player* human : sRandomPlayerbotMgr->GetPlayers())
        {
            if (!IsHumanPlayer(human) || !human->IsInWorld())
                continue;

            uint32 const humanLow = human->GetGUIDLow();
            BeginHumanLocked(humanLow, now);
            if (credit == 0)
                continue;

            Group* group = human->GetGroup();
            if (group && !group->isBGGroup() && !group->isBFGroup() && !human->InBattlegroundQueue())
            {
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                {
                    Player* member = ref->GetSource();
                    if (!member || member == human || !IsRandomBot(member))
                        continue;

                    uint32 const steps = groupClock.Add(LikingBook::PairClock::KeyOf(member->GetGUIDLow(), humanLow), credit,
                        groupStepSeconds);
                    if (steps)
                        RecordLocked(member, humanLow, LikingPolicy::Event::GroupTime, steps, now);
                }
            }

            if (uint32 const guildId = human->GetGuildId())
            {
                if (!guildsBuilt)
                {
                    guildsBuilt = true;
                    for (Player* bot : sRandomPlayerbotMgr->GetPlayerBotsSnapshot())
                        if (bot && bot->GetGuildId() && IsRandomBot(bot))
                            botsByGuild[bot->GetGuildId()].push_back(bot);
                }

                auto const members = botsByGuild.find(guildId);
                if (members == botsByGuild.end())
                    continue;

                for (Player* bot : members->second)
                {
                    uint32 const steps = guildClock.Add(LikingBook::PairClock::KeyOf(bot->GetGUIDLow(), humanLow), credit,
                        GUILD_STEP_SECONDS);
                    if (steps)
                        RecordLocked(bot, humanLow, LikingPolicy::Event::GuildTime, steps, now);
                }
            }
        }

        groupClock.EndPass();
        guildClock.EndPass();
    }

    // Counts the rows of one character in both tables, for the wipe command's report.
    uint64 CountRows(std::string const& table, std::string const& where)
    {
        // A plain Query: the formatted overload also matches a pointer argument as a connection.
        std::string const sql = "SELECT COUNT(*) FROM `" + table + "` WHERE " + where;
        QueryResult result = PlayerbotsDatabase.Query(sql.c_str());
        return result ? result->Fetch()[0].GetUInt64() : 0;
    }
}  // namespace

bool LikingStore::Enabled()
{
    return sPlayerbotAIConfig->socialLikingEnable && sPlayerbotAIConfig->enabled;
}

void LikingStore::Update()
{
    if (!Enabled())
        return;

    uint32 const now = Now();
    if (now == lastTickAt)
        return;
    lastTickAt = now;

    std::lock_guard<std::mutex> guard(storeLock);
    EnsureTablesLocked();
    PollLoadsLocked(now);

    if (nextRetentionAt == 0 || now >= nextRetentionAt)
    {
        RetentionLocked(now);
        nextRetentionAt = now + LikingPolicy::SECONDS_PER_DAY;
    }

    if (now < nextPassAt)
        return;
    nextPassAt = now + PASS_SECONDS;

    SocialPassLocked(now);
    FlushLocked();
}

void LikingStore::OnHumanLogin(Player* player)
{
    if (!Enabled() || !IsHumanPlayer(player))
        return;

    std::lock_guard<std::mutex> guard(storeLock);
    BeginHumanLocked(player->GetGUIDLow(), Now());
}

void LikingStore::OnHumanLogout(Player* player)
{
    // The hook runs for bots too; only a human has rows to write.
    if (!IsHumanPlayer(player))
        return;

    // Also with the switch off now: rows changed while it was on still go to the database.
    std::lock_guard<std::mutex> guard(storeLock);
    if (!tablesReady)
        return;

    uint32 const low = player->GetGUIDLow();
    loading.erase(low);
    std::vector<std::pair<uint32, uint32>> offers;
    std::vector<LikingBook::Row> const rows = book.End(low, &offers);
    if (!rows.empty() || !offers.empty())
        WriteLocked(rows, offers);
}

void LikingStore::Shutdown()
{
    SocialLog::Shutdown();
    std::lock_guard<std::mutex> guard(storeLock);
    if (!tablesReady)
        return;

    shuttingDown = true;
    std::vector<std::pair<uint32, uint32>> offers;
    std::vector<LikingBook::Row> const rows = book.TakeDirty(0, size_t(-1), &offers);
    if (!rows.empty() || !offers.empty())
        WriteLocked(rows, offers);
}

void LikingStore::OnPlayerDeleted(uint64 guid)
{
    uint32 const low = GUID_LOPART(guid);
    if (!low)
        return;

    std::lock_guard<std::mutex> guard(storeLock);
    // The privacy paths run with the switch off as well, so rows written while it was on are not left behind.
    EnsureTablesLocked();
    book.Wipe(low);
    book.ForgetBot(low);
    ExecuteAll(LikingBook::ForgetPlayerSql(low, true));
}

void LikingStore::NoteEvent(Player* first, Player* second, LikingPolicy::Event event, uint32 steps)
{
    if (!Enabled() || steps == 0)
        return;

    Player* bot = nullptr;
    Player* human = nullptr;
    if (!Resolve(first, second, bot, human))
    {
        if (LeftOutByScope(first, second))
            SocialLog::Count(SocialSoak::Counter::OutOfScope);
        return;
    }

    uint32 const now = Now();
    uint32 const humanLow = human->GetGUIDLow();
    std::lock_guard<std::mutex> guard(storeLock);
    // A human the book has not seen (the switch was turned on at runtime) is begun now; the event waits for the rows.
    BeginHumanLocked(humanLow, now);
    RecordLocked(bot, humanLow, event, steps, now);
}

void LikingStore::OnDungeonDone(Player* bot)
{
    if (!Enabled() || !IsRandomBot(bot))
        return;

    Group* group = bot->GetGroup();
    if (!group)
        return;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (Player* member = ref->GetSource(); IsHumanPlayer(member))
            NoteEvent(bot, member, LikingPolicy::Event::DungeonDone);
}

void LikingStore::OnTraded(Player* first, Player* second)
{
    NoteEvent(first, second, LikingPolicy::Event::Traded);
}

void LikingStore::OnDuelEnded(Player* winner, Player* loser)
{
    NoteEvent(winner, loser, LikingPolicy::Event::Dueled);
}

void LikingStore::OnHumanLine(Player* human, Player* whisperTarget, Group* group, std::string const& message)
{
    if (!Enabled() || !IsHumanPlayer(human))
        return;

    AiChatLines::Tone const tone = AiChatLines::ToneOf(message);
    if (tone == AiChatLines::Tone::NONE)
        return;

    // Who the line is for: the whispered bot, a bot of the group named in the line, else the bot the human has targeted.
    Player* bot = IsRandomBot(whisperTarget) ? whisperTarget : nullptr;
    if (!bot && group)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref && !bot; ref = ref->next())
            if (Player* member = ref->GetSource(); IsRandomBot(member) && AiChatPolicy::MentionsName(message, member->GetName()))
                bot = member;
    }
    if (!bot)
    {
        if (uint64 const target = human->GetTarget())
        {
            Player* targeted = ObjectAccessor::FindPlayer(target);
            if (IsRandomBot(targeted) && human->IsInWorld() && targeted->IsInWorld() && targeted->GetMapId() == human->GetMapId() &&
                human->GetDistance(targeted) <= TARGET_LINE_YARDS)
                bot = targeted;
        }
    }
    if (!bot)
        return;

    uint64 const key = LikingBook::PairClock::KeyOf(bot->GetGUIDLow(), human->GetGUIDLow());
    bool const thanked = tone == AiChatLines::Tone::THANKED;
    {
        std::lock_guard<std::mutex> guard(storeLock);
        if (!(thanked ? thanksGate : insultGate).Allow(key, Now(), TONE_WINDOW_SECONDS))
            return;
    }

    NoteEvent(bot, human, thanked ? LikingPolicy::Event::Thanked : LikingPolicy::Event::Insulted);
}

void LikingStore::OnGroupMemberAdded(Group* group, uint64 guid)
{
    if (!Enabled() || !group || group->isBGGroup() || group->isBFGroup())
        return;

    Player* joined = ObjectAccessor::FindPlayer(guid);
    if (!IsRandomBot(joined))
        return;

    std::lock_guard<std::mutex> guard(storeLock);
    if (joinedAt.size() >= JOIN_BOOK_LIMIT)
        joinedAt.clear();
    joinedAt[joined->GetGUIDLow()] = Now();
}

void LikingStore::OnGroupMemberRemoved(Group* group, uint64 guid, uint8 method, uint64 kicker)
{
    if (!Enabled() || !group || group->isBGGroup() || group->isBFGroup())
        return;

    Player* member = ObjectAccessor::FindPlayer(guid);
    if (!IsRandomBot(member))
        return;

    uint32 joined = 0;
    {
        std::lock_guard<std::mutex> guard(storeLock);
        auto const itr = joinedAt.find(member->GetGUIDLow());
        if (itr != joinedAt.end())
        {
            joined = itr->second;
            joinedAt.erase(itr);
        }
    }

    if (method != uint8(GROUP_REMOVEMETHOD_KICK) && method != uint8(GROUP_REMOVEMETHOD_KICK_LFG))
        return;

    Player* kicked = kicker ? ObjectAccessor::FindPlayer(kicker) : nullptr;
    if (!IsHumanPlayer(kicked) || !LikingPolicy::KickCounts(Tuning(), joined, Now()))
        return;

    NoteEvent(member, kicked, LikingPolicy::Event::Kicked);
}

int32 LikingStore::InviteDelta(Player* bot, Player* human)
{
    if (!Enabled())
        return 0;

    Player* resolvedBot = nullptr;
    Player* resolvedHuman = nullptr;
    if (!Resolve(bot, human, resolvedBot, resolvedHuman))
        return 0;

    uint32 const humanLow = resolvedHuman->GetGUIDLow();
    std::lock_guard<std::mutex> guard(storeLock);
    if (!book.Loaded(humanLow))
        return 0;

    return LikingPolicy::InviteChanceDelta(Tuning(), book.Value(Tuning(), humanLow, resolvedBot->GetGUIDLow(), Now()));
}

bool LikingStore::MayOfferGuild(Player* leader, Player* human, uint32 cooldownSeconds)
{
    // With AiPlayerbot.Social.OnlyBots set, a leader that is not on the list makes no offer, whatever the liking switch says.
    if (LeftOutByScope(leader, human))
    {
        SocialLog::Count(SocialSoak::Counter::OutOfScope);
        return false;
    }

    if (!Enabled())
        return true;

    Player* bot = nullptr;
    Player* person = nullptr;
    if (!Resolve(leader, human, bot, person))
        return false;

    uint32 const now = Now();
    uint32 const humanLow = person->GetGUIDLow();
    std::lock_guard<std::mutex> guard(storeLock);
    BeginHumanLocked(humanLow, now);
    if (!book.Loaded(humanLow))
        return false;

    LikingPolicy::Tuning const& tuning = Tuning();
    bool const taken = LikingPolicy::GuildOfferAllowed(tuning, book.Value(tuning, humanLow, bot->GetGUIDLow(), now)) &&
        LikingPolicy::OfferCooldownPassed(book.GuildOfferAt(humanLow), now, cooldownSeconds) &&
        book.TakeGuildOffer(humanLow, now, tuning.guildOffersPerHour);
    // Refusals are counted only: the guild pass asks every few seconds for each recruiting leader in reach.
    SocialLog::Count(taken ? SocialSoak::Counter::GuildOffer : SocialSoak::Counter::GuildOfferRefused);
    if (taken)
        SocialLog::Offer(SocialSoak::OfferLine("guild", bot->GetName(), humanLow, 0, true));
    return taken;
}

std::optional<double> LikingStore::ValueOf(Player* bot, Player* human)
{
    if (!Enabled())
        return std::nullopt;

    Player* resolvedBot = nullptr;
    Player* resolvedHuman = nullptr;
    if (!Resolve(bot, human, resolvedBot, resolvedHuman))
        return std::nullopt;

    uint32 const humanLow = resolvedHuman->GetGUIDLow();
    std::lock_guard<std::mutex> guard(storeLock);
    if (!book.Loaded(humanLow))
        return std::nullopt;

    return book.Value(Tuning(), humanLow, resolvedBot->GetGUIDLow(), Now());
}

bool LikingStore::MayOfferQuestGroup(Player* bot, Player* human, uint32 pairCooldownSeconds)
{
    if (!Enabled())
        return false;

    Player* resolvedBot = nullptr;
    Player* resolvedHuman = nullptr;
    if (!Resolve(bot, human, resolvedBot, resolvedHuman))
        return false;

    uint32 const humanLow = resolvedHuman->GetGUIDLow();
    std::lock_guard<std::mutex> guard(storeLock);
    // A human who is online and was never seen starts loading now; the offer waits for the next pass.
    BeginHumanLocked(humanLow, Now());
    bool const taken = book.TakeQuestOffer(Tuning(), humanLow, resolvedBot->GetGUIDLow(), Now(), pairCooldownSeconds);
    if (taken)
        for (LikingBook::QuestOffer const& offer : book.TakeQuestDirty(humanLow))
            PlayerbotsDatabase.Execute(LikingBook::QuestOfferInsertSql(offer).c_str());
    return taken;
}

std::vector<std::string> LikingStore::Console(std::string const& args)
{
    std::istringstream words(args);
    std::string action;
    std::string target;
    words >> action >> target;

    if (action != "wipe" || target.empty())
        return { "Usage: .playerbots social wipe <character name>|all",
            "Deletes the liking rows and offer times kept for that character (as a human and as a bot), or for everyone." };

    std::lock_guard<std::mutex> guard(storeLock);
    EnsureTablesLocked();

    if (target == "all")
    {
        uint64 const rows = CountRows("playerbots_social_liking", "1 = 1");
        uint64 const offers = CountRows("playerbots_social_offer", "1 = 1") +
            CountRows("playerbots_social_quest_offer", "1 = 1");
        book.WipeAll();
        ExecuteAll({ "DELETE FROM `playerbots_social_liking`", "DELETE FROM `playerbots_social_offer`",
            "DELETE FROM `playerbots_social_quest_offer`" });
        return { "Liking wipe: removed " + std::to_string(rows) + " rows and " + std::to_string(offers) + " offer times for every character.",
            AiChat::WipeSidecar(std::nullopt) };
    }

    std::string name = target;
    if (!normalizePlayerName(name))
        return { "Liking wipe: '" + target + "' is not a valid character name." };

    uint64 const fullGuid = sObjectMgr->GetPlayerGUIDByName(name);
    uint32 const low = GUID_LOPART(fullGuid);
    if (!low)
        return { "Liking wipe: no character named " + name + "." };

    std::string const where = "`player_guid` = " + std::to_string(low) + " OR `bot_guid` = " + std::to_string(low);
    uint64 const rows = CountRows("playerbots_social_liking", where);
    uint64 const offers = CountRows("playerbots_social_offer", "`player_guid` = " + std::to_string(low)) +
        CountRows("playerbots_social_quest_offer", where);
    book.Wipe(low);
    book.ForgetBot(low);
    ExecuteAll(LikingBook::ForgetPlayerSql(low, true));
    return { "Liking wipe: removed " + std::to_string(rows) + " rows and " + std::to_string(offers) + " offer times of " + name + ".",
        AiChat::WipeSidecar(fullGuid) };
}
