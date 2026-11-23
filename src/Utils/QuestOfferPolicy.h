/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_QUEST_OFFER_POLICY_H
#define AWARE_BOTS_QUEST_OFFER_POLICY_H

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <vector>

// Design decision 0033 point 16: a random bot that is working on the same quest as a human nearby, and likes them enough, offers
// to group up. The structural test of one bot and one human, the shared quest and the order the bots are tried in; the
// liking gate, the cooldown per pair and the cap per human per hour live in LikingStore (LikingPolicy, LikingBook).
// docs/social/social.md, slice 3. Keys AiPlayerbot.Social.QuestOffer.* and AiPlayerbot.Social.Liking.*.
namespace QuestOfferPolicy
{
struct Tuning
{
    // A bot offers from this far (yards, same map and zone).
    float radiusYards = 60.0f;
    // Seconds between two passes over the humans.
    uint32_t passSeconds = 30;
    // The same bot does not offer the same human again for this long.
    uint32_t pairCooldownSeconds = 6 * 3600;
    // Levels apart that still make a group worth it.
    uint32_t levelTolerance = 5;
};

enum class Verdict : uint8_t
{
    Offer,
    BotBusy,    // in combat, dead, in a battleground or an instance, in flight, or already in a group of its own with others
    HumanBusy,  // in combat, dead, DND, in a battleground or an instance, in a group, or in flight
    OtherZone,  // another map or zone
    TooFar,
    LevelGap,
    NoSharedQuest,
    OutOfScope,  // the bot is not on AiPlayerbot.Social.OnlyBots (a soak limited to named bots)
};

// What the game side reads about the pair. Everything is a plain value so the decision is tested without a core.
struct Facts
{
    bool botBusy = false;
    // The bot has other members in its group.
    bool botGrouped = false;
    bool humanBusy = false;
    bool humanGrouped = false;
    bool sameMap = true;
    bool sameZone = true;
    float yards = 0.0f;
    uint32_t botLevel = 1;
    uint32_t humanLevel = 1;
    bool sharedQuest = false;
    // False when AiPlayerbot.Social.OnlyBots names bots and this one is not among them.
    bool inScope = true;
};

inline Verdict Decide(Tuning const& tuning, Facts const& facts)
{
    if (facts.humanBusy || facts.humanGrouped)
        return Verdict::HumanBusy;
    if (facts.botBusy || facts.botGrouped)
        return Verdict::BotBusy;
    if (!facts.sameMap || !facts.sameZone)
        return Verdict::OtherZone;
    if (facts.yards > tuning.radiusYards)
        return Verdict::TooFar;
    uint32_t const gap = facts.botLevel > facts.humanLevel ? facts.botLevel - facts.humanLevel : facts.humanLevel - facts.botLevel;
    if (gap > tuning.levelTolerance)
        return Verdict::LevelGap;
    if (!facts.sharedQuest)
        return Verdict::NoSharedQuest;
    if (!facts.inScope)
        return Verdict::OutOfScope;
    return Verdict::Offer;
}

// The lowest quest id that is in both lists, or 0. A stable pick, so the same two logs always name the same quest in the
// whisper.
inline uint32_t FirstSharedQuest(std::vector<uint32_t> first, std::vector<uint32_t> second)
{
    std::sort(first.begin(), first.end());
    std::sort(second.begin(), second.end());
    size_t i = 0;
    size_t j = 0;
    while (i < first.size() && j < second.size())
    {
        if (first[i] == 0)
            ++i;
        else if (second[j] == 0)
            ++j;
        else if (first[i] == second[j])
            return first[i];
        else if (first[i] < second[j])
            ++i;
        else
            ++j;
    }
    return 0;
}

// One candidate bot of a human, with its distance in yards.
struct Candidate
{
    uint32_t bot = 0;
    float yards = 0.0f;
};

// The bots in the order they are tried: the nearest first, ties by guid, so the pass is deterministic and the closest bot
// is the one that speaks.
inline void OrderNearestFirst(std::vector<Candidate>& candidates)
{
    std::sort(candidates.begin(), candidates.end(), [](Candidate const& a, Candidate const& b)
    {
        return a.yards != b.yards ? a.yards < b.yards : a.bot < b.bot;
    });
}
}  // namespace QuestOfferPolicy

#endif
