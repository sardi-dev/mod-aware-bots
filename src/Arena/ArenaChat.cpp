/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ArenaChat.h"

#include "BotVoice.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Random.h"

#include <map>
#include <string>

namespace ArenaLadder
{
namespace Chat
{
namespace
{
using ArenaChatMath::Moment;

constexpr uint32 PERCENT = 100;

struct MomentText
{
    char const* key;
    char const* fallback;
};

// The default lines, used when the text table has no row of the key. Placeholders: %team, %opp, %rating, %streak, %record.
MomentText TextOf(Moment moment)
{
    switch (moment)
    {
        case Moment::FORMED:
            return { "arn_formed", "%team has formed up for the 2v2 pit, rated %rating to start" };
        case Moment::STREAK3:
            return { "arn_streak3", "that is three in a row for %team, %rating now" };
        case Moment::STREAK5:
            return { "arn_streak5", "five wins in a row for %team, %rating and climbing" };
        case Moment::STREAK_BREAK:
            return { "arn_streak_break", "%team just ended the streak of %opp" };
        case Moment::CLIMB:
            return { "arn_climb", "%team is up to %rating, near the top of the pit books" };
        case Moment::RIVAL:
            return { "arn_rival", "%team and %opp again, %record between them this fortnight" };
        case Moment::NONE:
            break;
    }
    return { "", "" };
}

// A bot that can speak in a General channel now: online, alive, in a city or the open world.
Player* SpeakerOf(Team const& team)
{
    Player* best = nullptr;
    uint32 bestChat = 0;
    for (Member const& member : team.members)
    {
        Player* bot = ObjectAccessor::FindPlayer(BotGuid::Create<BotHighGuid::Player>(member.guid));
        if (!bot || !bot->IsInWorld() || !bot->IsAlive() || bot->InBattleground() || bot->InBattlegroundQueue() || bot->InArena())
            continue;
        Map* map = bot->GetMap();
        if (!map || map->Instanceable() || !GET_PLAYERBOT_AI(bot))
            continue;

        uint32 const chat = BotVoice::ChattinessPercent(bot);
        if (!best || chat > bestChat || (chat == bestChat && bot->GetGUIDLow() < best->GetGUIDLow()))
        {
            best = bot;
            bestChat = chat;
        }
    }
    return best;
}

void Say(Moment moment, Team const& speakerTeam, Team const& subject, Team const* opponent, int32 streak, std::string const& record,
    int64 utcNow)
{
    State& st = S();
    Config cfg;
    bool quiet = false;
    {
        std::lock_guard<std::mutex> guard(st.lock);
        cfg = st.cfg;
    }
    if (!cfg.chatEnable)
        return;

    ArenaMath::LocalTime const local = OwnerNow(cfg, utcNow);
    quiet = ArenaMath::IsQuietHour(local.hour);
    if (quiet)
        return;

    Player* speaker = SpeakerOf(speakerTeam);
    if (!speaker)
        return;

    // The personality's chattiness decides whether this bot mentions it at all (quiet bots seldom do).
    if (urand(1, PERCENT) > std::min(PERCENT, BotVoice::ChattinessPercent(speaker)))
        return;

    {
        std::lock_guard<std::mutex> guard(st.lock);
        if (!st.chat.TryTake(utcNow, quiet, subject.id))
            return;
    }

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(speaker);
    if (!botAI)
        return;

    MomentText const text = TextOf(moment);
    std::map<std::string, std::string> const placeholders = {
        { "%team", subject.name },
        { "%opp", opponent ? opponent->name : std::string() },
        { "%rating", std::to_string(std::max(0, subject.rating)) },
        { "%streak", std::to_string(std::abs(streak)) },
        { "%record", record },
    };
    BotVoice::Say(botAI, text.key, text.fallback, placeholders, BotVoice::Channel::GENERAL, BotVoice::Priority::NOTICE, PERCENT, nullptr,
        true);
}
}  // namespace

void OnFormed(Team const& team, int64 utcNow)
{
    Say(Moment::FORMED, team, team, nullptr, 0, std::string(), utcNow);
}

void OnWon(Team const& winner, Team const& loser, ArenaChatMath::ResultFacts const& facts, int64 utcNow)
{
    Moment const moment = ArenaChatMath::ChooseMoment(facts, true);
    if (moment == Moment::NONE)
        return;

    std::string const record = std::to_string(facts.winsOfTeam) + "-" + std::to_string(facts.winsOfOpponent);
    Say(moment, winner, winner, &loser, facts.newStreak, record, utcNow);
}

void OnStreakBroken(Team const& winner, Team const& loser, int32 loserStreak, int64 utcNow)
{
    Say(Moment::STREAK_BREAK, winner, winner, &loser, loserStreak, std::string(), utcNow);
}
}  // namespace Chat
}  // namespace ArenaLadder
