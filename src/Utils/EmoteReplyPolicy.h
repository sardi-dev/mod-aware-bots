/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_EMOTE_REPLY_POLICY_H
#define AWARE_BOTS_EMOTE_REPLY_POLICY_H

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <unordered_map>

// Design decision 0033 point 17: a human who emotes at a random bot gets a gesture back, so a city feels inhabited. The decision
// logic only (docs/social/social.md, slice 3): the game side reads the emote packet, asks Decide and plays the gesture.
// Keys AiPlayerbot.Social.EmoteReplies.*, default off.
namespace EmoteReplyPolicy
{
struct Tuning
{
    // An emote aimed at the bot (it is the target of the emote) is answered from this far.
    float radiusYards = 20.0f;
    // An emote with no target at all is answered only by a bot this close, so a crowd does not all turn around.
    float untargetedYards = 8.0f;
    // One reply per human in this time from all bots together.
    uint32_t perHumanSeconds = 60;
    // Percent of untargeted emotes that get a reply at all.
    uint32_t untargetedChancePct = 35;
};

enum class Aim : uint8_t
{
    AtBot,     // the human targeted this bot and emoted
    AtNobody,  // an emote with no target
    AtOther,   // aimed at someone else
};

enum class Verdict : uint8_t
{
    Reply,
    NotAimed,  // the emote was for someone else
    TooFar,
    OutOfScope,  // the bot is not on AiPlayerbot.Social.OnlyBots (a soak limited to named bots)
    Busy,      // in combat, dead, in a battleground or an instance, in flight, casting
    Cold,      // the bot does not like this human (liking band cold)
    Chance,    // an untargeted emote that lost its roll
    Limited,   // this human already got a reply within perHumanSeconds
};

struct Facts
{
    Aim aim = Aim::AtNobody;
    float yards = 0.0f;
    // False when AiPlayerbot.Social.OnlyBots names bots and this one is not among them.
    bool inScope = true;
    bool busy = false;
    bool cold = false;
    // 1 to 100, from the game's random number source.
    uint32_t roll = 100;
    bool limited = false;
};

inline Verdict Decide(Tuning const& tuning, Facts const& facts)
{
    if (facts.aim == Aim::AtOther)
        return Verdict::NotAimed;
    if (facts.yards > tuning.radiusYards || (facts.aim == Aim::AtNobody && facts.yards > tuning.untargetedYards))
        return Verdict::TooFar;
    if (!facts.inScope)
        return Verdict::OutOfScope;
    if (facts.busy)
        return Verdict::Busy;
    if (facts.cold)
        return Verdict::Cold;
    if (facts.aim == Aim::AtNobody && facts.roll > tuning.untargetedChancePct)
        return Verdict::Chance;
    if (facts.limited)
        return Verdict::Limited;
    return Verdict::Reply;
}

// The last reply per human (by low guid), in unix seconds. The check and the record are two calls: the gesture is only
// recorded when it really played, so an emote the table has no answer for does not use up the minute.
class Limiter
{
public:
    bool Limited(uint32_t human, uint32_t now, uint32_t gapSeconds) const
    {
        auto const itr = last.find(human);
        if (itr == last.end() || gapSeconds == 0)
            return false;
        // A clock set back never blocks for good.
        return now >= itr->second && now - itr->second < gapSeconds;
    }

    void Record(uint32_t human, uint32_t now, uint32_t gapSeconds)
    {
        if (last.size() >= MAX_KEPT)
            Trim(now, gapSeconds);
        last[human] = now;
    }

    size_t Size() const
    {
        return last.size();
    }

private:
    static constexpr size_t MAX_KEPT = 256;

    void Trim(uint32_t now, uint32_t gapSeconds)
    {
        for (auto itr = last.begin(); itr != last.end();)
            itr = (now < itr->second || now - itr->second >= gapSeconds) ? last.erase(itr) : std::next(itr);
        if (last.size() >= MAX_KEPT)
            last.clear();
    }

    std::unordered_map<uint32_t, uint32_t> last;
};
}  // namespace EmoteReplyPolicy

#endif
