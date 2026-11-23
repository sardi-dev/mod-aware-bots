/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_LIKING_POLICY_H
#define AWARE_BOTS_LIKING_POLICY_H

#include <algorithm>
#include <cmath>
#include <cstdint>

// Design decision 0033 point 14 and 16: how much a random bot likes one human player. A score in [0, 1] per (bot, human) pair
// that moves with shared moments (grouping, a finished dungeon, a trade, a duel, guild time, thanks, rudeness), fades
// back to a neutral value when nothing happens, and gates the offers a bot makes (a group for a shared quest, a guild).
// The decision logic only: the clock is an argument (unix seconds), nothing here reads a database or a game object.
// docs/social/social.md has the design and the config keys (AiPlayerbot.Social.Liking.*).
namespace LikingPolicy
{
// Every number a server owner may tune. The defaults are the designed values; config reads the same names.
struct Tuning
{
    // Where a pair with no history sits, and where an old score drifts back to.
    double neutral = 0.30;
    double decayHalfLifeDays = 21.0;
    // Positive moves of one pair add up to this much per day, so a long grouping session or repeated trades cannot
    // farm the score in a night. Rude moves are never capped.
    double dailyGainCap = 0.15;
    // Full minutes of grouping that make one GroupTime step.
    uint32_t groupStepMinutes = 10;

    // Score change of one step of each event.
    double groupStep = 0.01;
    double dungeonDone = 0.06;
    double traded = 0.02;
    double dueled = 0.02;
    double guildHour = 0.01;
    double thanked = 0.02;
    double insulted = -0.10;
    double kicked = -0.08;
    double lootNinja = -0.15;

    // Band edges: below coldBelow the bot is cold to the human, from friendlyAt it is a friend, from closeAt a close one.
    double coldBelow = 0.20;
    double friendlyAt = 0.55;
    double closeAt = 0.75;

    // Percent points of group-invite chance per 1.0 of score above or below neutral (GoalInvites adds this next to its
    // own friend and guild bonuses). 60 makes a friend worth +15, the same as the flat FriendBonus.
    double inviteSpan = 60.0;

    // The lowest score at which a bot offers a human a group for a shared quest, or a guild.
    double questOfferMin = 0.40;
    double guildOfferMin = 0.55;
    // Offers one human gets per hour, from all bots together.
    uint32_t questOffersPerHour = 2;
    uint32_t guildOffersPerHour = 1;

    // A kick this soon after the bot joined the group counts for nothing: a mis-click or a bot invited by mistake.
    uint32_t kickGraceSeconds = 120;
};

enum class Event : uint8_t
{
    GroupTime,   // steps: GroupSteps(minutes grouped)
    DungeonDone, // a finder or party run finished with the human
    Traded,      // a completed trade window
    Dueled,      // a duel that was fought to its end
    GuildTime,   // steps: full hours online in the same guild
    Thanked,     // an obvious thanks in a line to the bot
    Insulted,    // an obvious insult in a line to the bot
    Kicked,      // the human removed the bot from a group it did not misbehave in
    LootNinja,   // the human needed an item the bot wanted and could not use
};

// One row of the liking table. gainDay and gainToday hold the positive moves already counted today (day = unix
// seconds / 86400), so the daily cap survives a restart.
struct Pair
{
    double value = 0.30;
    uint32_t updatedAt = 0;
    uint32_t gainDay = 0;
    double gainToday = 0.0;
};

enum class Band : uint8_t
{
    Cold,
    Stranger,
    Friendly,
    Close,
};

constexpr uint32_t SECONDS_PER_DAY = 86400;
constexpr uint32_t SECONDS_PER_HOUR = 3600;

inline double Clamp01(double value)
{
    return std::min(1.0, std::max(0.0, value));
}

inline Pair Fresh(Tuning const& tuning, uint32_t now)
{
    Pair pair;
    pair.value = Clamp01(tuning.neutral);
    pair.updatedAt = now;
    return pair;
}

// The score after elapsedSeconds with nothing happening: the distance to neutral halves every decayHalfLifeDays, from
// either side, so a grudge fades as a friendship does.
inline double Decayed(Tuning const& tuning, double value, uint32_t elapsedSeconds)
{
    double const neutral = Clamp01(tuning.neutral);
    if (tuning.decayHalfLifeDays <= 0.0)
        return neutral;
    double const halfLives = double(elapsedSeconds) / (tuning.decayHalfLifeDays * double(SECONDS_PER_DAY));
    return neutral + (Clamp01(value) - neutral) * std::exp2(-halfLives);
}

// The pair as it stands at `now`, with the decay applied; a clock that went backwards counts as no time passed.
inline Pair Settled(Tuning const& tuning, Pair pair, uint32_t now)
{
    uint32_t const elapsed = now > pair.updatedAt ? now - pair.updatedAt : 0;
    pair.value = Decayed(tuning, pair.value, elapsed);
    if (now > pair.updatedAt)
        pair.updatedAt = now;
    return pair;
}

inline double ValueAt(Tuning const& tuning, Pair const& pair, uint32_t now)
{
    return Settled(tuning, pair, now).value;
}

inline uint32_t GroupSteps(Tuning const& tuning, uint32_t minutesGrouped)
{
    return tuning.groupStepMinutes ? minutesGrouped / tuning.groupStepMinutes : 0;
}

inline double DeltaOf(Tuning const& tuning, Event event)
{
    switch (event)
    {
        case Event::GroupTime: return tuning.groupStep;
        case Event::DungeonDone: return tuning.dungeonDone;
        case Event::Traded: return tuning.traded;
        case Event::Dueled: return tuning.dueled;
        case Event::GuildTime: return tuning.guildHour;
        case Event::Thanked: return tuning.thanked;
        case Event::Insulted: return tuning.insulted;
        case Event::Kicked: return tuning.kicked;
        case Event::LootNinja: return tuning.lootNinja;
    }
    return 0.0;
}

// The pair after `steps` of an event at `now`: decay first, then the change. A positive change is cut to what is left of
// the day's cap; a negative one is not. The result stays in [0, 1].
inline Pair Apply(Tuning const& tuning, Pair pair, Event event, uint32_t steps, uint32_t now)
{
    pair = Settled(tuning, pair, now);
    uint32_t const day = now / SECONDS_PER_DAY;
    if (pair.gainDay != day)
    {
        pair.gainDay = day;
        pair.gainToday = 0.0;
    }

    double delta = DeltaOf(tuning, event) * double(steps);
    if (delta > 0.0)
    {
        double const room = std::max(0.0, tuning.dailyGainCap - pair.gainToday);
        delta = std::min(delta, room);
        pair.gainToday += delta;
    }

    pair.value = Clamp01(pair.value + delta);
    return pair;
}

inline Band BandOf(Tuning const& tuning, double value)
{
    if (value < tuning.coldBelow)
        return Band::Cold;
    if (value < tuning.friendlyAt)
        return Band::Stranger;
    if (value < tuning.closeAt)
        return Band::Friendly;
    return Band::Close;
}

// The band word the AI sidecar's prompt carries (design decision 0029: game context only).
inline char const* BandName(Band band)
{
    switch (band)
    {
        case Band::Cold: return "cold";
        case Band::Stranger: return "stranger";
        case Band::Friendly: return "friendly";
        case Band::Close: return "close";
    }
    return "stranger";
}

// The whole percent the sidecar request carries next to the band word.
inline uint32_t Percent(double value)
{
    return uint32_t(std::lround(Clamp01(value) * 100.0));
}

// Percent points GoalInvites adds to an invite from this human: negative below neutral, positive above.
inline int32_t InviteChanceDelta(Tuning const& tuning, double value)
{
    return int32_t(std::lround((Clamp01(value) - Clamp01(tuning.neutral)) * tuning.inviteSpan));
}

inline bool QuestOfferAllowed(Tuning const& tuning, double value)
{
    return value >= tuning.questOfferMin;
}

inline bool GuildOfferAllowed(Tuning const& tuning, double value)
{
    return value >= tuning.guildOfferMin;
}

// Offers one human got in the current hour, one window per human and kind. The window opens at the first offer and
// closes SECONDS_PER_HOUR later; the next offer then opens a new one.
struct OfferWindow
{
    uint32_t openedAt = 0;
    uint32_t count = 0;

    // True and counted when the human may be offered something now; false when the cap for this window is reached.
    bool TryTake(uint32_t now, uint32_t capPerHour)
    {
        if (capPerHour == 0)
            return false;
        if (count == 0 || now < openedAt || now - openedAt >= SECONDS_PER_HOUR)
        {
            openedAt = now;
            count = 0;
        }
        if (count >= capPerHour)
            return false;
        ++count;
        return true;
    }
};

// A score this close to neutral (after decay) holds nothing worth keeping; the SQL cleanup uses the same number.
constexpr double FORGET_DUST = 0.02;

// Rows whose score sits this close to neutral and that were not touched for retentionDays hold nothing worth keeping.
inline bool Forgettable(Tuning const& tuning, Pair const& pair, uint32_t now, uint32_t retentionDays)
{
    uint32_t const age = now > pair.updatedAt ? now - pair.updatedAt : 0;
    return age >= retentionDays * SECONDS_PER_DAY &&
        std::fabs(ValueAt(tuning, pair, now) - Clamp01(tuning.neutral)) <= FORGET_DUST;
}

// Only a random bot and a human player make a pair: never two bots, never a player's own alt bot, never the same guid.
inline bool PairEligible(bool botIsRandomBot, bool otherIsHuman, uint32_t botGuid, uint32_t humanGuid)
{
    return botIsRandomBot && otherIsHuman && botGuid != 0 && humanGuid != 0 && botGuid != humanGuid;
}

// A kick lowers the score unless the bot had been in the group for less than kickGraceSeconds. A bot whose join time is
// unknown (joinedAt 0, for example a group that outlived a restart) is past the grace.
inline bool KickCounts(Tuning const& tuning, uint32_t joinedAt, uint32_t now)
{
    if (joinedAt == 0 || now < joinedAt)
        return true;
    return now - joinedAt >= tuning.kickGraceSeconds;
}

// True when a human may be offered the same kind of thing again: nothing was offered yet, or the cooldown ran out. A stored
// time far in the future (a clock set back) does not block for good.
inline bool OfferCooldownPassed(uint32_t lastOfferAt, uint32_t now, uint32_t cooldownSeconds)
{
    if (lastOfferAt == 0)
        return true;
    if (now < lastOfferAt)
        return lastOfferAt - now > cooldownSeconds;
    return now - lastOfferAt >= cooldownSeconds;
}
}  // namespace LikingPolicy

#endif
