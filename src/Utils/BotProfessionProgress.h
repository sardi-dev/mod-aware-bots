#ifndef BOT_PROFESSION_PROGRESS_H
#define BOT_PROFESSION_PROGRESS_H

#include <cstdint>

// BT-197, design decision 0016/0017: random bots earn their professions like players. The factory only picks the pair a bot
// will learn; the bot learns each rank at a trainer with its own money, and the skill value rises only through the
// core's normal skill-ups (gathering, crafting). These are the pure rules the factory and the trainer visit share.
namespace BotProfessionProgress
{
// The profession pair the factory chose for a random bot (the "firstSkill" / "secondSkill" values), with whether each
// id is a primary trade skill.
struct StoredPair
{
    std::uint32_t first;
    bool firstPrimary;
    std::uint32_t second;
    bool secondPrimary;
};

// Whether a random bot may learn a trade skill trainer spell of `skill`. Secondary skills (cooking, first aid, fishing,
// archaeology) are open to every bot. A primary skill must be one of the stored pair; with no stored pair only ranks of
// a profession the bot already has.
inline bool MayTrain(bool primary, bool secondary, std::uint32_t skill, StoredPair const& pair, bool hasSkill)
{
    if (secondary)
        return true;

    if (!primary || !skill)
        return false;

    if ((pair.firstPrimary && skill == pair.first) || (pair.secondPrimary && skill == pair.second))
        return true;

    if (pair.firstPrimary || pair.secondPrimary)
        return false;

    return hasSkill;
}

// The primary profession slots still free: the realm's limit minus the primary professions the bot really knows.
// Chosen but unlearned professions keep their slot free, or the trainer would refuse their first rank.
inline std::uint32_t FreePrimarySlots(std::uint32_t maxPrimary, std::uint32_t knownPrimary)
{
    return maxPrimary > knownPrimary ? maxPrimary - knownPrimary : 0;
}
}

#endif
