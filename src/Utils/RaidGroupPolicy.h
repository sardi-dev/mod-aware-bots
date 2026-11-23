/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_RAIDGROUPPOLICY_H
#define _PLAYERBOT_RAIDGROUPPOLICY_H

// The pure rules of the `botscenario <player> raidgroup` test verb: which online bots join a test raid group around a probe.
// A battleground test needs more than the 5 a party holds (the Eye of the Storm capture bar clamp needs 6 players of one
// team at one base); the verb converts the probe's group to a raid and adds bots of the probe's faction.
// Pure logic, free of game types (tests/test_raid_group_policy.cpp). The glue is Scenario/BotScenarioQueue.cpp.

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace RaidGroupPolicy
{
// A party holds this many; one more member makes it a raid.
constexpr std::uint32_t PARTY_SIZE = 5;
// A raid group of the 5.4.8 core.
constexpr std::uint32_t RAID_SIZE = 40;

struct Candidate
{
    std::uint32_t guid = 0;
    std::uint8_t classId = 0;
    std::uint8_t level = 0;
    bool sameFaction = false;
    bool isBot = false;
    bool grouped = false;
    bool inBattleground = false;
    bool inInstance = false;
    bool alive = true;
    bool excluded = false;
};

inline bool Eligible(Candidate const& c, std::uint8_t minLevel, std::uint8_t maxLevel)
{
    return c.isBot && c.sameFaction && !c.grouped && !c.inBattleground && !c.inInstance && c.alive && !c.excluded &&
           c.level >= minLevel && c.level <= maxLevel;
}

// The room left for new members in a group that holds `members` already: a party up to its 5 seats, a raid up to 40.
// A group that has to grow past the party size is converted to a raid first.
inline std::uint32_t Room(std::uint32_t members, bool raid)
{
    std::uint32_t const cap = raid ? RAID_SIZE : PARTY_SIZE;
    return members >= cap ? 0u : cap - members;
}

inline bool NeedsRaid(std::uint32_t membersAfter)
{
    return membersAfter > PARTY_SIZE;
}

// Picks `want` bots: eligible candidates only, one class at a time in turn (so a short list still has healers and damage
// dealers), the lower guid first inside a class, so the same server state gives the same group. Returns the guids.
inline std::vector<std::uint32_t> Pick(std::vector<Candidate> const& all, std::uint32_t want, std::uint8_t minLevel,
                                       std::uint8_t maxLevel)
{
    std::vector<Candidate> pool;
    for (Candidate const& c : all)
        if (Eligible(c, minLevel, maxLevel))
            pool.push_back(c);
    std::sort(pool.begin(), pool.end(), [](Candidate const& a, Candidate const& b)
              { return a.classId != b.classId ? a.classId < b.classId : a.guid < b.guid; });

    std::vector<std::uint32_t> picked;
    std::vector<bool> used(pool.size(), false);
    while (picked.size() < want)
    {
        bool tookAny = false;
        std::uint8_t lastClass = 0;
        bool haveLast = false;
        for (std::size_t i = 0; i < pool.size() && picked.size() < want; ++i)
        {
            if (used[i] || (haveLast && pool[i].classId == lastClass))
                continue;
            used[i] = true;
            picked.push_back(pool[i].guid);
            lastClass = pool[i].classId;
            haveLast = true;
            tookAny = true;
        }
        if (!tookAny)
            break;
    }
    return picked;
}
}  // namespace RaidGroupPolicy

#endif
