#ifndef AWARE_BOTS_QUEST_LEVELING_POLICY_H
#define AWARE_BOTS_QUEST_LEVELING_POLICY_H

#include <cstddef>
#include <vector>

// How a random bot levels with a purpose (BT-231, owner 2026-11-16: "sensible quests for their level and zone, finish
// them, hand them in, move on to the next zone, no aimless moves"). The rpg status draw stays random among what is
// left; these rules only take the aimless choices out of it.
namespace QuestLevelingPolicy
{
// Which logged quest the DO_QUEST pick works on: a hand-in first (the experience comes with it), then a quest at the
// bot's level, a low-level quest only when nothing else is left in reach.
enum class Tier
{
    HandIn = 0,
    AtLevel = 1,
    LowLevel = 2
};

inline Tier QuestTier(bool complete, bool lowLevel)
{
    if (complete)
        return Tier::HandIn;
    return lowLevel ? Tier::LowLevel : Tier::AtLevel;
}

// Indexes of the entries in the best tier present; the caller draws one of them at random.
inline std::vector<std::size_t> BestTier(std::vector<Tier> const& tiers)
{
    std::vector<std::size_t> best;
    for (std::size_t i = 0; i < tiers.size(); ++i)
    {
        if (!best.empty() && tiers[i] > tiers[best.front()])
            continue;
        if (!best.empty() && tiers[i] < tiers[best.front()])
            best.clear();
        best.push_back(i);
    }
    return best;
}

// BT-373: within the best tier, the quests whose POI is about as near as the nearest one (the caller draws one of
// them). The draw was random over the whole tier, so a bot crossed its zone between two quests and back, the zig-zag
// the owner saw; a player works the quest at hand first.
constexpr float NEAR_SLACK_YARDS = 100.0f;

inline std::vector<std::size_t> NearestOfTier(std::vector<std::size_t> const& tier, std::vector<float> const& dists)
{
    std::vector<std::size_t> nearest;
    if (tier.empty())
        return nearest;
    float best = dists[tier.front()];
    for (std::size_t i : tier)
        if (dists[i] < best)
            best = dists[i];
    for (std::size_t i : tier)
        if (dists[i] <= best + NEAR_SLACK_YARDS)
            nearest.push_back(i);
    return nearest;
}

// BT-373: with only low-level quests left in reach, the bot first looks for a giver with a quest at its level (the
// quest hub search) and works the grey quest only when there is none, as a player would.
inline bool LookForBetterFirst(Tier best)
{
    return best == Tier::LowLevel;
}

// BT-232: when the bot's zone has no quest left to take, the quest hub search looks this much farther and through this
// many of the nearest givers for one with an at-level quest in any zone, so the bot walks on to the next zone.
constexpr float NEXT_ZONE_RADIUS_FACTOR = 2.0f;
constexpr std::size_t NEXT_ZONE_GIVERS_CHECKED = 64;

// The rpg statuses that move a bot away from its quests.
enum class Draw
{
    Quest,
    Grind,
    Flight,
    Other
};

// While the bot has quest work in this zone (a doable quest in the log or a giver with one), it neither flies off to a
// random zone of its level nor grinds instead: grinding is the fallback when the quests run out, and the flight is how
// it moves on to the next zone then.
inline bool KeepInDraw(Draw draw, bool questWorkHere)
{
    if (!questWorkHere)
        return true;
    return draw != Draw::Grind && draw != Draw::Flight;
}
}  // namespace QuestLevelingPolicy

#endif
