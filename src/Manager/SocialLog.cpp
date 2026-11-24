/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * See SocialLog.h and docs/social/social.md section 13.
 */

#include "SocialLog.h"

#include "Log.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "RandomPlayerbotMgr.h"

#include <ctime>
#include <atomic>
#include <array>
#include "LikingStore.h"

namespace
{
    std::array<std::atomic<uint64>, SocialSoak::COUNTER_COUNT> counts{};
    SocialSoak::Tally totals;
    SocialSoak::PassStats passStats;
    uint32 lastSummaryAt = 0;

    bool AnySwitchOn()
    {
        return sPlayerbotAIConfig->socialLikingEnable || sPlayerbotAIConfig->socialPlaystyleEnable ||
            sPlayerbotAIConfig->socialQuestOfferEnable || sPlayerbotAIConfig->socialEmoteRepliesEnable ||
            sPlayerbotAIConfig->socialTradeChatterEnable;
    }

    // Called only on the world thread or during shutdown.
    void WriteSummary(uint32 now)
    {
        SocialSoak::Tally interval;
        for (size_t i = 0; i < SocialSoak::COUNTER_COUNT; ++i)
        {
            auto const counter = SocialSoak::Counter(i);
            uint64 const value = counts[i].exchange(0, std::memory_order_relaxed);
            interval.Add(counter, value);
            totals.Add(counter, value);
        }
        uint32 const window = lastSummaryAt && now >= lastSummaryAt ? now - lastSummaryAt : 0;
        auto const tiers = LikingStore::TierCounts(now);
        SocialSoak::StyleCounts styles{};
        for (Player* bot : sRandomPlayerbotMgr->GetPlayerBotsSnapshot())
            if (bot && sRandomPlayerbotMgr->IsRandomBot(bot))
                SocialSoak::CountStyle(styles, bot->GetGUIDLow(), sPlayerbotAIConfig->socialPlaystyleShares);
        SF_LOG_INFO("playerbots", "%s", SocialSoak::SummaryLine(sPlayerbotAIConfig->socialOnlyBots, interval, window, tiers,
            totals.Get(SocialSoak::Counter::QuestOffer), totals.Get(SocialSoak::Counter::DbWrites), passStats, styles).c_str());
        passStats = SocialSoak::PassStats();
        lastSummaryAt = now;
    }
}  // namespace

bool SocialLog::InScope(Player* bot)
{
    return bot && sPlayerbotAIConfig->socialOnlyBots.Allows(bot->GetName());
}

void SocialLog::Count(SocialSoak::Counter counter, uint64 amount)
{
    counts[size_t(counter)].fetch_add(amount, std::memory_order_relaxed);
}

void SocialLog::QuestPass(uint32 elapsedMs, uint64 candidates)
{
    passStats.Add(elapsedMs, candidates);
}

void SocialLog::Action(std::string const& line)
{
    if (sPlayerbotAIConfig->socialSoakLog)
        SF_LOG_INFO("playerbots", "%s", line.c_str());
    else
        SF_LOG_DEBUG("playerbots", "%s", line.c_str());
}

void SocialLog::Offer(std::string const& line)
{
    SF_LOG_INFO("playerbots", "%s", line.c_str());
}

void SocialLog::Update()
{
    if (!AnySwitchOn())
        return;

    uint32 const now = uint32(time(nullptr));
    if (lastSummaryAt == 0)
    {
        // The first call starts the window: no line for a server that has just started.
        lastSummaryAt = now;
        return;
    }

    if (SocialSoak::SummaryDue(lastSummaryAt, now, sPlayerbotAIConfig->socialSummarySeconds, true))
        WriteSummary(now);
}

void SocialLog::Shutdown()
{
    if (!AnySwitchOn())
        return;

    WriteSummary(uint32(time(nullptr)));
}
