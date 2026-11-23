/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * See SocialLog.h and docs/social/social.md section 13.
 */

#include "SocialLog.h"

#include "Log.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"

#include <ctime>
#include <mutex>

namespace
{
    std::mutex tallyLock;
    SocialSoak::Tally tally;
    SocialSoak::Tally lastWritten;
    uint32 lastSummaryAt = 0;

    bool AnySwitchOn()
    {
        return sPlayerbotAIConfig->socialLikingEnable || sPlayerbotAIConfig->socialPlaystyleEnable ||
            sPlayerbotAIConfig->socialQuestOfferEnable || sPlayerbotAIConfig->socialEmoteRepliesEnable;
    }

    // Under the lock. The window is the time since the last summary, or since the first call.
    void WriteSummaryLocked(uint32 now)
    {
        uint32 const window = lastSummaryAt && now >= lastSummaryAt ? now - lastSummaryAt : 0;
        SF_LOG_INFO("playerbots", "%s", SocialSoak::SummaryLine(sPlayerbotAIConfig->socialOnlyBots, tally, window).c_str());
        lastWritten = tally;
        lastSummaryAt = now;
    }
}  // namespace

bool SocialLog::InScope(Player* bot)
{
    return bot && sPlayerbotAIConfig->socialOnlyBots.Allows(bot->GetName());
}

void SocialLog::Count(SocialSoak::Counter counter, uint64 amount)
{
    std::lock_guard<std::mutex> guard(tallyLock);
    tally.Add(counter, amount);
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
    std::lock_guard<std::mutex> guard(tallyLock);
    if (lastSummaryAt == 0)
    {
        // The first call starts the window: no line for a server that has just started.
        lastSummaryAt = now;
        return;
    }

    if (SocialSoak::SummaryDue(lastSummaryAt, now, sPlayerbotAIConfig->socialSummarySeconds, !(tally == lastWritten)))
        WriteSummaryLocked(now);
}

void SocialLog::Shutdown()
{
    if (!AnySwitchOn())
        return;

    std::lock_guard<std::mutex> guard(tallyLock);
    WriteSummaryLocked(uint32(time(nullptr)));
}
